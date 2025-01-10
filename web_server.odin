package main

import "core:os"
import "core:strings"
import "core:fmt"
import "core:net"

Mime_Type :: enum{
    Default,
    Avif,
    Css,
    Csv,
    Gif,
    Html,
    Ico,
    Jpeg,
    Js,
    Json,
    Png,
    Pdf,
    Svg,
    Txt,
    Webp,
    Xml
}

Mime_Types := [Mime_Type]string{
    .Default  = "application/octet-stream",
    .Avif     = "image/avif",
    .Css      = "text/css",
    .Csv      = "text/csv",
    .Gif      = "image/gif",
    .Html     = "text/html",
    .Ico      = "image/vnd.microsoft.icon",
    .Jpeg     = "image/jpeg",
    .Js       = "text/javascript",
    .Json     = "application/json",
    .Png      = "image/png",
    .Pdf      = "application/pdf",
    .Svg      = "image/svg+xml",
    .Txt      = "text/plain",
    .Webp     = "image/webp",
    .Xml      = "application/xml"
}
Mime_ends := [Mime_Type][]string{
    .Default  = nil,
    .Avif     = {".avif"},
    .Css      = {".css"},
    .Csv      = {".csv"},
    .Gif      = {".gif"},
    .Html     = {".htm", ".html"},
    .Ico      = {".ico"},
    .Jpeg     = {".jpg", ".jpeg"},
    .Js       = {".js", ".mjs"},
    .Json     = {".json"},
    .Png      = {".png"},
    .Pdf      = {".pdf"},
    .Svg      = {".svg"},
    .Txt      = {".txt"},
    .Webp     = {".webp"},
    .Xml      = {".xml"}
}

get_content_type :: proc(path :string) -> Mime_Type {
    last_dot := strings.last_index(path, ".")

    if last_dot == -1 {
        return .Default
    }
    path := path[last_dot:]

    for i in 1..<len(Mime_Type) {
        me := Mime_ends[Mime_Type(i)]
        if me == nil {
            continue
        }
        for end in me {
            if strings.compare(path, end) == 0 {
                return Mime_Type(i)
            }
        }
    }
    return .Default
}

MAX_REQUEST_SIZE :: 2047
Client_Info :: struct{
    address  :net.Address,
    socket   :net.TCP_Socket,
    request  :[MAX_REQUEST_SIZE+1]byte,
    received :int,
    next     :^Client_Info,
}
clients :^Client_Info = nil;

get_client :: proc(socket :net.TCP_Socket) -> Client_Info {
    ci := clients;
    for {
        if ci == nil {
            break
        }
        if ci.socket == socket {
            return ci
        }
        ci = ci.next
    }
    n := new(Client_Info)
    n.next = clients
    clients = n;
    return n

}

main :: proc() {

}
