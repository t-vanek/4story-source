#pragma once

// OperatorSession — wraps a ControlSession with the per-operator
// state that legacy CTManager carries (m_bManager / m_bAuthority /
// m_dwID / m_strID / m_bLock). One instance per accepted operator
// TCP connection. The handler chain owns the post-login state
// machine; OperatorSession just holds the fields.

#include "control_session.h"

#include <cstdint>
#include <memory>
#include <string>

namespace tcontrolsvr {

enum class OperatorRole : std::uint8_t
{
    // Mirrors MANAGER_CLASS in TControlType.h. Numeric values match
    // the legacy SP return so they can travel over the wire as-is.
    None      = 0,
    All       = 1,    // MANAGER_ALL — patch upload, full control (127.0.0.1 only)
    Control   = 2,    // patch / upload
    User      = 3,    // user kick / position / movement
    Service   = 4,    // ON/OFF
    GMLevel1  = 5,    // GM tool — all use
    GMLevel2  = 6,    // GM tool — restricted
    GMLevel3  = 7,
};

class OperatorSession : public std::enable_shared_from_this<OperatorSession>
{
public:
    explicit OperatorSession(std::shared_ptr<ControlSession> sess)
        : m_sess(std::move(sess)) {}

    const std::shared_ptr<ControlSession>& Wire() const { return m_sess; }

    // Set on successful CT_OPLOGIN_REQ / CT_STLOGIN_REQ.
    void MarkLoggedIn(std::string user_id,
                      std::uint8_t authority_raw,
                      std::uint32_t manager_seq)
    {
        m_user_id      = std::move(user_id);
        m_authority    = authority_raw;
        m_manager_seq  = manager_seq;
        m_logged_in    = true;
    }

    bool         LoggedIn() const     { return m_logged_in; }
    std::uint8_t AuthorityRaw() const { return m_authority; }
    OperatorRole Role() const         { return static_cast<OperatorRole>(m_authority); }
    std::uint32_t ManagerSeq() const  { return m_manager_seq; }
    const std::string& UserId() const { return m_user_id; }

    // Locked operator can't issue commands (legacy m_bLock). Currently
    // unused by F1 handlers; reserved for the admin shell + GM
    // protection flow that lands in F3.
    bool IsLocked() const  { return m_locked; }
    void SetLocked(bool v) { m_locked = v; }

private:
    std::shared_ptr<ControlSession>  m_sess;
    bool          m_logged_in   = false;
    bool          m_locked      = false;
    std::uint8_t  m_authority   = 0;
    std::uint32_t m_manager_seq = 0;
    std::string   m_user_id;
};

} // namespace tcontrolsvr
