/* REXX */

"@rm -f nls.cns"

do forever
    line = linein("nls.h")
    if left(line, 10) == "#define M_" then,
        rc = lineout("nls.cns", '{"'word(line, 2)'", 'word(line, 2)'},')
    if stream("nls.h") <> "READY" then leave
end
