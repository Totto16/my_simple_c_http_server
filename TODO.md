- [ ] header  Upgrade-Insecure-Requests: 1
- [ ] HTML URL Encoding (%20 for space)

reserved    = ";" | "/" | "?" | ":" | "@" | "&" | "=" | "+" |"$" | ","
https://www.w3schools.com/tags/ref_urlencode.ASP
https://datatracker.ietf.org/doc/html/rfc2396

-> %20 goes to char_form_code(20) alias ascii

- [ ] http/2


- [ ] http tests
https://github.com/pevalme/HTTPValidator
https://github.com/vfaronov/httpolice/
https://github.com/summerwind/h2spec
https://httpd.apache.org/test/
https://github.com/apache/httpd-tests
curl tests?

- [ ] android support: note, not that easily possible


features:

 allow multilepl rouet, main http --route, fore test servers!


use zen c z-libs zvec instead of stdds and also zmap and maybe also zlist!

- use string views, use from assparserc, for oparsing config file and also generic parsing, (optional write generic text parser?, for http and config parsing)

- us enginx like config, for ftp / ws / http / ftp handling and setup!)


- parse uri correctly, add tests for that

- add these headers to files:


Date: Sat, 31 Jan 2026 09:37:10 GMT
Last-Modified: Sat, 31 Jan 2026 02:17:18 GMT
ETag: "697d662e-c3f"


- head or range request for files should not read the file!

