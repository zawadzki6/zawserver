## <p align="center">zawserver</p>
Zawadzki's Anti-Windows Server - "zawserver" (written in lowercase) is a server for serving static web content through HTTP.

[See it in action here](http://zaw-softworks.org/index.html)

#### Features
- C89 compatibility
- Simple stupid design
- Small size, portable binary
- No external libraries - made using glibc
- Common media type support

#### Roadmap
- implement supplementary HTTP methods (HEAD, OPTIONS, TRACE) - done
- handle **all** crashes - in progress/done(?)
- simultaneous connections
- configuration options:
    - redirects
    - customization
    - custom limits
- code cleanup - in progress
- blocking useragents/IPs

There are no plans to port zawserver to another system such as Wind*ws, make zawserver interactive or implement methods such as POST.

There are also no plans to implement HTTPS.
