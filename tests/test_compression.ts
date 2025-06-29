//TODO: test compression e.g.

//

// curl -s -H 'Accept-Encoding: deflate' "http://localhost:8080/json" | openssl zlib -d

// curl -s -H 'Accept-Encoding: gzip' "http://localhost:8080/json" | gunzip

// curl -s -H 'Accept-Encoding: br' "http://localhost:8080/json" | brotli -d --stdout

// curl -s -H 'Accept-Encoding: zstd' "http://localhost:8080/json" | zstd -d
