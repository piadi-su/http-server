---

this is a simple http server written in c
using only posix sockets.

the server uses select() for it's I/O multiplexing
and has a single verb support `GET`.

### Supported HTTP Status Codes
| Code | Status | Trigger Condition |
| :--- | :--- | :--- |
| **200** | `OK` | File found and served successfully. |
| **404** | `Not Found` | Requested file does not exist or tries to escape the web root. |
| **405** | `Method Not Allowed` | Received any HTTP method other than `GET` (e.g., `POST`, `PUT`). |
| **500** | `Internal Server Error` | Parsing errors or internal execution failures. |

---


