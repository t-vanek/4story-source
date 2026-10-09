// Owned, isolated integration fixture. Never forwards mail or logs its contents.
import net from 'node:net';
import fs from 'node:fs';
import crypto from 'node:crypto';
const spool = process.argv[2];
const server = net.createServer(socket => {
  let buffer = '', recipient = '', data = null;
  socket.setEncoding('utf8');
  socket.on('error', () => {});
  socket.write('220 disposable.example.invalid ESMTP\r\n');
  socket.on('data', chunk => {
    buffer += chunk;
    if (buffer.length > 65536) { socket.destroy(); return; }
    while (buffer.includes('\n')) {
      const end = buffer.indexOf('\n');
      const line = buffer.slice(0, end).replace(/\r$/, ''); buffer = buffer.slice(end + 1);
      if (data !== null) {
        if (line === '.') {
          fs.writeFileSync(`${spool}/${crypto.randomUUID()}.json`, JSON.stringify({recipient, message: data}), {mode: 0o600});
          data = null; socket.write('250 accepted locally\r\n');
        } else {
          data += line.replace(/^\.\./, '.') + '\n';
          if (data.length > 65536) socket.destroy();
        }
      } else if (/^EHLO /i.test(line)) {
        // Intentionally coalesced: production parser must retain buffered lines.
        socket.write('250-disposable.example.invalid\r\n250-8BITMIME\r\n250 HELP\r\n');
      } else if (/^(HELO |MAIL FROM:)/i.test(line)) socket.write('250 ok\r\n');
      else if (/^RCPT TO:/i.test(line)) {
        recipient = line.slice(8).replace(/^<|>$/g, '');
        socket.write('250 ok\r\n');
      } else if (line === 'DATA') {
        if (recipient === 'synthetic221@example.invalid') {
          fs.writeFileSync(`${spool}/stalled`, '', {mode: 0o600});
          // A deliberately silent relay exercises production deadline/shutdown.
        } else { data = ''; socket.write('354 send message\r\n'); }
      } else if (line === 'QUIT') socket.end('221 bye\r\n');
      else socket.write('500 unexpected fixture command\r\n');
    }
  });
});
server.listen(2525, '0.0.0.0', () => fs.writeFileSync(`${spool}/ready`, '', {mode: 0o600}));
