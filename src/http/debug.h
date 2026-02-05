

#pragma once

#include "./protocol.h"
#include "utils/string_builder.h"
#include "utils/utils.h"

// returning a stringbuilder, that makes a string from the httpRequest, this is useful for debugging
NODISCARD StringBuilder* http_request_to_string_builder(HttpRequest request, bool https);

// really simple and dumb html boilerplate, this is used for demonstration purposes, and is
// static, but it looks"cool" and has a shutdown button, that works (with XMLHttpRequest)

NODISCARD StringBuilder*
html_from_string(StringBuilder* head_content, // NOLINT(bugprone-easily-swappable-parameters)
                 StringBuilder* script_content, StringBuilder* style_content,
                 StringBuilder* body_content);

NODISCARD StringBuilder* http_request_to_json(HttpRequest request, bool https,
                                              SendSettings send_settings);

// really simple and dumb html boilerplate, this is used for demonstration purposes, and is
// static, but it looks"cool" and has a shutdown button, that works (with XMLHttpRequest)
NODISCARD StringBuilder* http_request_to_html(HttpRequest request, bool https,
                                              SendSettings send_settings);
