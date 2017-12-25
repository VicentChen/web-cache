# Web-Cache
---
##RFC Specification
[RFC 2616 - HTTP Specification][1]
[RFC 7230 - Message Syntax and Routing][2]
[RFC 1123 - Time format][3]

###Web Cache Behavior
![Web Cache Behavior][4]

###What is a Proxy
>An intermediary program which acts as both a server and a client for the purpose of making requests on behalf of other clients. 

>Requests are serviced internally or by passing them on, with possible translation, to other servers. A proxy MUST implement both the client and server requirements of this specification. 

>A "**transparent proxy**" is a proxy that does not modify the request or response beyond what is required for proxy authentication and identification. 

>A "**non-transparent proxy**" is a proxy that modifies the request or response in order to provide some added service to the user agent, such as group annotation services, media type transformation, protocol reduction, or anonymity filtering. 

>Except where either transparent or non-transparent behavior is explicitly stated, the HTTP proxy requirements apply to both types of proxies.
###Semantically transparent
>A cache behaves in a "semantically transparent" manner, with respect to a particular response, when its use affects neither the requesting client nor the origin server, except to improve performance. 

>When a cache is semantically transparent, **the client receives exactly the same response (except for hop-by-hop headers) that it would have received had its request been handled directly by the origin server**.

##HTTP Message Format
```
generic-message = start-line
                  *(message-header CRLF)
                  CRLF
                  [ message-body ]
start-line      = Request-Line | Status-Line
```
##Request-Line & Status-Line
```
Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
```
##Header
```
header-field   = field-name ":" OWS field-value OWS
OWS            = *( SP / HTAB )
               ; optional whitespace
field-name     = token
               ; field-names are case-sensitive
field-value    = *( field-content / obs-fold )
field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
field-vchar    = VCHAR / obs-text
obs-fold       = CRLF 1*( SP / HTAB )
               ; obsolete line folding
token          = 1*tchar
tchar          = "!" / "#" / "$" / "%" / "&" / "'" / "*"
               / "+" / "-" / "." / "^" / "_" / "`" / "|" / "~"
               / DIGIT / ALPHA
               ; any VCHAR, except delimiters
```
###Host
>A client MUST include a Host header field in all HTTP/1.1 request
   messages
###If-Modified-Since
>The semantics of the GET method change to a "conditional GET" if the request message includes an **If-Modified-Since**, **If-Unmodified-Since**, **If-Match**, **If-None-Match**, or **If-Range** header field. 

```
If-Modified-Since = "If-Modified-Since" ":" HTTP-date
```

>HTTP-date is **case sensitive** and **MUST NOT include additional LWS** beyond that specifically included as SP in the grammar.

```
HTTP-date    = rfc1123-date | rfc850-date | asctime-date
rfc1123-date = wkday "," SP date1 SP time SP "GMT"
rfc850-date  = weekday "," SP date2 SP time SP "GMT"
asctime-date = wkday SP date3 SP time SP 4DIGIT
date1        = 2DIGIT SP month SP 4DIGIT
               ; day month year (e.g., 02 Jun 1982)
date2        = 2DIGIT "-" month "-" 2DIGIT
               ; day-month-year (e.g., 02-Jun-82)
date3        = month SP ( 2DIGIT | ( SP 1DIGIT ))
               ; month day (e.g., Jun  2)
time         = 2DIGIT ":" 2DIGIT ":" 2DIGIT
               ; 00:00:00 - 23:59:59
wkday        = "Mon" | "Tue" | "Wed"
             | "Thu" | "Fri" | "Sat" | "Sun"
weekday      = "Monday" | "Tuesday" | "Wednesday"
             | "Thursday" | "Friday" | "Saturday" | "Sunday"
month        = "Jan" | "Feb" | "Mar" | "Apr"
             | "May" | "Jun" | "Jul" | "Aug"
             | "Sep" | "Oct" | "Nov" | "Dec"
```


  [1]: https://tools.ietf.org/html/rfc2616
  [2]: https://tools.ietf.org/html/rfc7230
  [3]: https://tools.ietf.org/html/rfc1123
  [4]: https://mdn.mozillademos.org/files/13771/HTTPStaleness.png