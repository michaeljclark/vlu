#!/usr/bin/python

with open('vlu8-figure.tex', 'w') as fp:

    header = """
    \\documentclass{report}
    \\usepackage[letterpaper, portrait, margin=0.5in]{geometry}
    \\usepackage[utf8]{inputenc}
    \\usepackage{tikz}
    \\usepackage{lipsum}
    \\usepackage{mwe}
    \\usetikzlibrary{graphs}
    \\begin{document}
    \\begin{figure}[htp]
    \\centering
    \\paragraph{VLU8 - Variable Length Unary Integer Coding \\textit{(VLU, bits=8)}}
    \\paragraph{ }
    \\paragraph{ }
    \\begin{tikzpicture}[fill=blue!20,scale=0.25]
    \\begin{scope}[every node/.style={font=\\scriptsize}]
    """
    fp.write(header)

    for x in range(8, 72, 8):
      fp.write("\\fill [black!%d] (%d,-1)   rectangle (%d,-2);\n"
              % (((x>>3)%2)*10+10, x, x-8))
    fp.write("\n")
    for x in range(8, 72, 8):
      fp.write("\\draw [black!50] (%d,-1)   rectangle (%d,-2);\n"
              % (x, x-8))
    fp.write("\n")
    for x in range(0, 64, 8):
      fp.write("\\node (bit_%d)    at (%5.2f  ,-1.5) {$7$};\n"
                 % (63 - x, x + 0.5))
      fp.write("\\node (bit_%d)    at (%5.2f  ,-1.5) {$0$};\n"
                 % (56 - x, x + 7.5))
    fp.write("\n")
    for x in range(0,8,1):
        fp.write("\\node (byte_%d)     at (%f ,%f) {$byte-%d$};\n"
               % (x, 4 + (7-x)*8, 0, x + 1 ))
    fp.write("\n")
    for u in range(0, 8, 1):
        y = u
        fp.write("\\node (a_0)     at (69.0 ,%f) {$%d%sbit$};\n"
                 % (-2.5 - y, (u+1) * 7, '+' if u == 7 else '-'))
        for x in range(64, 64 - (u+1)*8, -8):
            s = ((x>>3)%2)*10+10
            fp.write("\\fill [black!%d] (%d,%f)   rectangle (%d,%f);\n"
              % (s, x, -2 - y, x-8, -3.0 - y))
            fp.write("\\draw [black!50] (%d,%d)   rectangle (%d,%d);\n"
              % (   x, -2 - y, x-8, -3.0 - y))
        fp.write("\\node (bit_0_%d)    at ( %f, %f ) {$%s$};\n"
                  % (u,   0.5 + (64-(u+1)), -2.5 - y, 'c' if u == 7 else '0' ));
        for x in range(1, u + 1, 1):
            fp.write("\\node (bit_%d_%d)    at ( %f, %f ) {$1$};\n"
                  % (u,x, 0.5 + (64-x), -2.5 - y ));
        for x in range(u + 2, u + (u + 1) * 7 + 2, 1):
            fp.write("\\node (bit_%d_%d)    at ( %f, %f ) {$n$};\n"
                  % (u,x, 0.5 + (64-x), -2.5 - y ));
    fp.write("\n")

    footer = """
    \\end{scope}
    \\end{tikzpicture}
    \\end{figure}
    \\centering
    \\begin{minipage}{0.45\\textwidth}
    \\begin{verbatim}
    struct vlu_result
    {
        uint64_t val;
        int64_t shamt;
    };

    struct vlu_result vlu_decode(uint64_t vlu)
    {
        int t1 = __builtin_ctzll(~vlu);
        bool cond = t1 > 7;
        int shamt = cond ? 8 : t1 + 1;
        uint64_t mask = ~(-!cond << (shamt << 3));
        uint64_t num = (vlu >> shamt) & mask;
        return (vlu_result) { num, shamt };
    }
    \\end{verbatim}
    \\end{minipage}\\hfill
    \\begin{minipage}{0.45\\textwidth}
    \\begin{verbatim}
    vlu_decode:
        mov     rcx, rdi
        not     rcx
        tzcnt   rcx, rcx
        xor     rsi, rsi
        cmp     rcx, 7
        setne   sil
        lea     rdx, [rcx + 1]
        mov     rcx, 8
        cmovg   rdx, rcx
        shrx    rax, rdi, rdx
        lea     rcx, [rdx * 8]
        neg     rsi
        shlx    rsi, rsi, rcx
        andn    rax, rsi, rax
        ret     ; struct { rax; rdx; }
    \\end{verbatim}
    \\end{minipage}
    \\end{document}
    """
    fp.write(footer)
