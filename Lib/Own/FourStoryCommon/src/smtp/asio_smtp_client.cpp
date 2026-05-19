#include "fourstory/smtp/asio_smtp_client.h"

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/error_code.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <istream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace fourstory::smtp {

namespace {

// RFC 2045 base64 encoder. Just enough for AUTH LOGIN — username +
// password are short, so the tiny inline implementation is fine.
std::string Base64Encode(const std::string& in)
{
    static const char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((in.size() + 2) / 3) * 4);
    std::size_t i = 0;
    while (i + 3 <= in.size())
    {
        const auto a = static_cast<unsigned char>(in[i]);
        const auto b = static_cast<unsigned char>(in[i + 1]);
        const auto c = static_cast<unsigned char>(in[i + 2]);
        out.push_back(kAlphabet[(a >> 2) & 0x3F]);
        out.push_back(kAlphabet[((a & 0x03) << 4) | ((b >> 4) & 0x0F)]);
        out.push_back(kAlphabet[((b & 0x0F) << 2) | ((c >> 6) & 0x03)]);
        out.push_back(kAlphabet[c & 0x3F]);
        i += 3;
    }
    if (i < in.size())
    {
        const auto a = static_cast<unsigned char>(in[i]);
        const unsigned char b = i + 1 < in.size()
            ? static_cast<unsigned char>(in[i + 1]) : 0;
        out.push_back(kAlphabet[(a >> 2) & 0x3F]);
        out.push_back(kAlphabet[((a & 0x03) << 4) | ((b >> 4) & 0x0F)]);
        if (i + 1 < in.size())
            out.push_back(kAlphabet[(b & 0x0F) << 2]);
        else
            out.push_back('=');
        out.push_back('=');
    }
    return out;
}

// SMTP transaction wrapper. Holds the socket, reads / writes complete
// requests, and parses the 3-digit reply codes. Throws std::runtime_error
// on any protocol violation so the caller can bail out via a single
// catch block in Send.
class Transaction
{
public:
    Transaction(boost::asio::io_context& io, std::chrono::seconds timeout)
        : m_io(io), m_socket(io), m_timeout(timeout) {}

    void Connect(const std::string& host, std::uint16_t port)
    {
        // Resolve + connect. Asio's resolver is synchronous in this
        // mode — the auth handler runs on its own coroutine, so a
        // blocking Send is acceptable for the 2FA path (low volume,
        // happens once per new device).
        boost::asio::ip::tcp::resolver res(m_io);
        const auto endpoints = res.resolve(host, std::to_string(port));
        boost::asio::connect(m_socket, endpoints);
    }

    // Send a CRLF-terminated command. The terminator is added here so
    // callers can write the human-readable command without remembering
    // to append \r\n. Empty command = just a CRLF (used after DATA
    // body's terminating dot).
    void WriteLine(const std::string& cmd)
    {
        std::string line = cmd;
        line += "\r\n";
        boost::asio::write(m_socket, boost::asio::buffer(line));
    }

    // Read one SMTP reply. Each reply is one or more lines of the form
    //   NNN<sp|->text
    // The final line has NNN<sp>, continuation lines have NNN-. Returns
    // the numeric code. Throws on closed socket or malformed reply.
    int ReadReply(std::string* full = nullptr)
    {
        std::string accumulated;
        int code = 0;
        for (;;)
        {
            boost::asio::streambuf buf;
            const auto n = boost::asio::read_until(m_socket, buf, "\r\n");
            if (n == 0) throw std::runtime_error("smtp: relay closed mid-reply");
            std::istream is(&buf);
            std::string line;
            std::getline(is, line);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.size() < 4)
                throw std::runtime_error("smtp: malformed reply '" + line + "'");
            try
            {
                code = std::stoi(line.substr(0, 3));
            }
            catch (...)
            {
                throw std::runtime_error("smtp: non-numeric reply code in '" + line + "'");
            }
            if (!accumulated.empty()) accumulated += '\n';
            accumulated += line;
            if (line[3] == ' ') break;  // last line
            if (line[3] != '-')
                throw std::runtime_error("smtp: malformed continuation in '" + line + "'");
        }
        if (full) *full = std::move(accumulated);
        return code;
    }

    // Send the message body in DATA mode. CRLF normalization + RFC 5321
    // §4.5.2 dot-stuffing (lines starting with '.' get an extra '.'
    // prepended so they aren't mistaken for the terminator).
    void WriteBody(const std::string& body)
    {
        std::string out;
        out.reserve(body.size() + 64);
        bool at_line_start = true;
        for (std::size_t i = 0; i < body.size(); ++i)
        {
            const char c = body[i];
            if (at_line_start && c == '.')
            {
                out.push_back('.');
                out.push_back('.');
                at_line_start = false;
                continue;
            }
            if (c == '\n')
            {
                // Normalize bare LF to CRLF; CRLF passes through
                // because the preceding CR was already emitted.
                if (out.empty() || out.back() != '\r') out.push_back('\r');
                out.push_back('\n');
                at_line_start = true;
                continue;
            }
            out.push_back(c);
            at_line_start = false;
        }
        if (out.empty() || out.back() != '\n')
        {
            if (out.empty() || out.back() != '\r') out.push_back('\r');
            out.push_back('\n');
        }
        // RFC 5321: DATA ends with a line containing a single dot.
        out.append(".\r\n");
        boost::asio::write(m_socket, boost::asio::buffer(out));
    }

    void Close()
    {
        boost::system::error_code ignore;
        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignore);
        m_socket.close(ignore);
    }

private:
    boost::asio::io_context&    m_io;
    boost::asio::ip::tcp::socket m_socket;
    std::chrono::seconds        m_timeout;
};

// Quick check whether an EHLO reply text advertises an AUTH LOGIN
// capability. Strict but tolerant of mixed case.
bool EhloHasAuthLogin(const std::string& ehlo_text)
{
    // Look for a line containing "AUTH" followed by " LOGIN" anywhere
    // before the next CRLF / end. EHLO replies aren't case-fixed.
    std::string lower;
    lower.reserve(ehlo_text.size());
    for (char c : ehlo_text)
        lower.push_back((c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c);
    // Find "auth" lines and check for "login" on the same line.
    std::size_t pos = 0;
    while (pos < lower.size())
    {
        auto eol = lower.find('\n', pos);
        if (eol == std::string::npos) eol = lower.size();
        const auto line = lower.substr(pos, eol - pos);
        if (line.find("auth") != std::string::npos
            && line.find("login") != std::string::npos)
        {
            return true;
        }
        pos = eol + 1;
    }
    return false;
}

} // namespace

AsioSmtpClient::AsioSmtpClient(AsioSmtpConfig config)
    : m_config(std::move(config))
{
    if (m_config.host.empty())
        throw std::runtime_error("AsioSmtpClient: smtp.host is empty");
    if (m_config.port == 0)
        throw std::runtime_error("AsioSmtpClient: smtp.port is 0");
    if (m_config.from_address.empty())
        throw std::runtime_error("AsioSmtpClient: smtp.from_address is empty");
    if (m_config.from_display.empty()) m_config.from_display = m_config.from_address;
}

bool AsioSmtpClient::Send(const std::string& to_address,
                          const std::string& subject,
                          const std::string& body)
{
    if (to_address.empty())
    {
        spdlog::warn("smtp.Send: empty to_address — refusing");
        return false;
    }

    boost::asio::io_context io;
    Transaction tx(io, m_config.io_timeout);

    try
    {
        tx.Connect(m_config.host, m_config.port);

        // 220 service ready banner.
        if (int code = tx.ReadReply(); code != 220)
            throw std::runtime_error("smtp: bad banner code " + std::to_string(code));

        // EHLO host. Pick a host literal — most relays accept the IP
        // address form. Tries the configured local hostname falling
        // back to "localhost"; relays don't usually enforce a strict
        // FQDN match for AUTH'd submissions.
        std::string ehlo_text;
        bool ehlo_ok = false;
        if (m_config.prefer_ehlo)
        {
            tx.WriteLine("EHLO tloginsvr");
            const int code = tx.ReadReply(&ehlo_text);
            ehlo_ok = (code >= 200 && code < 300);
        }
        if (!ehlo_ok)
        {
            tx.WriteLine("HELO tloginsvr");
            if (int code = tx.ReadReply(); code < 200 || code >= 300)
                throw std::runtime_error("smtp: HELO refused " + std::to_string(code));
            ehlo_text.clear();
        }

        // Optional AUTH LOGIN. Activated when:
        //   1. caller supplied creds AND
        //   2. (we used EHLO and the relay advertised AUTH LOGIN) OR
        //      (caller forces it by configuring creds without an EHLO
        //       capability list)
        const bool have_creds = !m_config.username.empty() && !m_config.password.empty();
        const bool relay_offers_auth = ehlo_ok && EhloHasAuthLogin(ehlo_text);
        if (have_creds && (relay_offers_auth || !ehlo_ok))
        {
            tx.WriteLine("AUTH LOGIN");
            if (int code = tx.ReadReply(); code != 334)
                throw std::runtime_error("smtp: AUTH LOGIN refused " + std::to_string(code));
            tx.WriteLine(Base64Encode(m_config.username));
            if (int code = tx.ReadReply(); code != 334)
                throw std::runtime_error("smtp: AUTH username refused " + std::to_string(code));
            tx.WriteLine(Base64Encode(m_config.password));
            if (int code = tx.ReadReply(); code != 235)
                throw std::runtime_error("smtp: AUTH password refused " + std::to_string(code));
        }

        // Envelope.
        tx.WriteLine("MAIL FROM:<" + m_config.from_address + ">");
        if (int code = tx.ReadReply(); code != 250)
            throw std::runtime_error("smtp: MAIL FROM refused " + std::to_string(code));
        tx.WriteLine("RCPT TO:<" + to_address + ">");
        if (int code = tx.ReadReply(); code != 250 && code != 251)
            throw std::runtime_error("smtp: RCPT TO refused " + std::to_string(code));

        // DATA.
        tx.WriteLine("DATA");
        if (int code = tx.ReadReply(); code != 354)
            throw std::runtime_error("smtp: DATA refused " + std::to_string(code));

        // Headers + body. We don't add MIME parts — the security-code
        // mail is a plain ASCII reminder, no attachments. Subject is
        // ASCII-only because the legacy login flow only ever sends
        // English copy.
        std::ostringstream msg;
        msg << "From: " << m_config.from_display
            << " <" << m_config.from_address << ">\r\n";
        msg << "To: " << to_address << "\r\n";
        msg << "Subject: " << subject << "\r\n";
        msg << "MIME-Version: 1.0\r\n";
        msg << "Content-Type: text/plain; charset=UTF-8\r\n";
        msg << "Content-Transfer-Encoding: 8bit\r\n";
        msg << "\r\n";
        msg << body;
        tx.WriteBody(msg.str());

        if (int code = tx.ReadReply(); code != 250)
            throw std::runtime_error("smtp: DATA body refused " + std::to_string(code));

        // Polite shutdown.
        tx.WriteLine("QUIT");
        tx.ReadReply();  // 221 — but don't fail the Send on a missing QUIT ack
        tx.Close();

        spdlog::info("smtp.Send to='{}' subject='{}' → ok ({}:{})",
            to_address, subject, m_config.host, m_config.port);
        return true;
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("smtp.Send to='{}' failed: {} ({}:{})",
            to_address, ex.what(), m_config.host, m_config.port);
        tx.Close();
        return false;
    }
}

} // namespace fourstory::smtp
