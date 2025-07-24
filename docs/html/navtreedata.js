/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "NWC Point of Sale", "index.html", [
    [ "Nostr Wallet Connect Point of Sale Device", "index.html", "index" ],
    [ "NWC Point of Sale Device Architecture", "md__a_r_c_h_i_t_e_c_t_u_r_e.html", [
      [ "Project Overview", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md13", [
        [ "Key Features", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md14", null ],
        [ "Technology Stack", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md15", null ]
      ] ],
      [ "Architecture Overview", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md17", null ],
      [ "File Structure and Module Descriptions", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md19", [
        [ "Core Application Files", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md20", [
          [ "<tt>src/main.cpp</tt>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md21", null ],
          [ "<tt>src/app.cpp</tt> / <tt>src/app.h</tt>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md22", null ]
        ] ],
        [ "Display and UI Modules", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md23", [
          [ "<tt>src/display.cpp</tt> / <tt>src/display.h</tt>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md24", null ],
          [ "<tt>src/ui.cpp</tt> / <tt>src/ui.h</tt>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md25", null ]
        ] ],
        [ "Connectivity Modules", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md26", [
          [ "<tt>src/wifi.cpp</tt> / <tt>src/wifi.h</tt>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md27", null ],
          [ "<tt>src/nwc.cpp</tt> / <tt>src/nwc.h</tt>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md28", null ]
        ] ],
        [ "Configuration and Storage", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md29", [
          [ "<tt>src/settings.cpp</tt> / <tt>src/settings.h</tt>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md30", null ],
          [ "<tt>src/config.h</tt>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md31", null ]
        ] ],
        [ "External Libraries", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md32", [
          [ "<tt>lib/TFT_eSPI/</tt>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md33", null ],
          [ "<tt>lib/aes/</tt>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md34", null ],
          [ "<tt>lib/nostr/</tt>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md35", null ]
        ] ]
      ] ],
      [ "Data Flow and Module Interactions", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md37", [
        [ "1. <strong>Application Startup</strong>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md38", null ],
        [ "2. <strong>Payment Processing Flow</strong>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md39", null ],
        [ "3. <strong>WiFi Configuration Flow</strong>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md40", null ],
        [ "4. <strong>Settings Management Flow</strong>", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md41", null ]
      ] ],
      [ "Communication Protocols", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md43", [
        [ "Nostr Wallet Connect (NWC)", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md44", null ],
        [ "Lightning Network", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md45", null ],
        [ "External APIs", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md46", null ]
      ] ],
      [ "Hardware Configuration", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md48", [
        [ "ESP32 Pin Mapping", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md49", null ],
        [ "Power Management", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md50", null ]
      ] ],
      [ "Security Features", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md52", [
        [ "Cryptographic Security", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md53", null ],
        [ "Network Security", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md54", null ]
      ] ],
      [ "Development and Build", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md56", [
        [ "PlatformIO Configuration", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md57", null ],
        [ "Key Dependencies", "md__a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md58", null ]
      ] ]
    ] ],
    [ "Modules", "modules.html", "modules" ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ],
        [ "Enumerator", "namespacemembers_eval.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", null ],
        [ "Functions", "functions_func.html", null ],
        [ "Variables", "functions_vars.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Enumerator", "globals_eval.html", null ],
        [ "Macros", "globals_defs.html", "globals_defs" ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"_a_x_s15231_b__touch_8cpp.html",
"globals_d.html",
"md__a_r_c_h_i_t_e_c_t_u_r_e.html",
"namespace_u_i.html#a5b4ed56a4755e396af5d6c827cba06d6",
"nostr_8cpp.html#ae7add69f32ac7f6af83399c69374cc50",
"struct_wi_fi_manager_1_1wifi__scan__result__t.html#a2cd58f9b8af709afd9b1c380f7a62cb9"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';