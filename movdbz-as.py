#!/usr/bin/env python

import argparse
import os
import re
import sys


class ParseError(Exception):
    def __init__(self, line_no, value):
        self.line_no = line_no
        self.value = value


def lex_line(line, line_no):
    token_exprs = [
        ('[ \\n\\t]+', None),
        (';[^\\n]*', None),
        ('movdbz', 'opcode'),
        ('r[1-9][0-9]*', 'reg'),
        ('r0', 'reg'),
        ('discard', 'reg'),
        (':', 'reserved'),
        (',', 'reserved'),
        ('0x[0-9]+', 'integer'),
        ('[1-9][0-9]*', 'integer'),
        ('0', 'integer'),
        ('[a-zA-Z][a-zA-Z0-9_]*', 'label')]
    pos = 0
    tokens = []
    while pos < len(line):
        match = None
        for token_expr in token_exprs:
            pattern, tag = token_expr
            regex = re.compile(pattern)
            match = regex.match(line, pos)
            if match:
                text = match.group(0)
                if tag:
                    token = (tag, text)
                    tokens.append(token)
                break
        if not match:
            raise ParseError(line_no, 'Syntax error')
        pos = match.end(0)
    return tokens


def parse_movdbz(tokens, line_no):
    if len(tokens) != 10:
        raise ParseError(line_no, 'Syntax error')
    if (tokens[3][0] != 'reg' or
            tokens[4][1] != ',' or
            (tokens[5][0] != 'reg' and tokens[5][0] != 'integer') or
            tokens[6][1] != ',' or
            tokens[7][0] != 'label' or
            tokens[8][1] != ',' or
            tokens[9][0] != 'label'):
        raise ParseError(line_no, 'Syntax error')
    return (line_no, tokens[0][1], tokens[2][1],
            tokens[3][1], tokens[5][1],
            tokens[7][1], tokens[9][1])


def parse_line(tokens, line_no):
    if len(tokens) < 3:
        raise ParseError(line_no, "Line must start with 'label: opcode'")
    if tokens[0][0] != 'label' or tokens[1][1] != ':':
        raise ParseError(line_no, "Line must start with 'label: opcode'")
    if tokens[0][1] == 'exit':
        raise ParseError(line_no, "exit is not a valid label name")
    if tokens[2][1] == 'movdbz':
        return parse_movdbz(tokens, line_no)
    else:
        raise ParseError(line_no, "Unknown opcode " + tokens[2][1])


def read_file(stream):
    insts = []
    line_no = 0
    for line in stream:
        line_no += 1
        tokens = lex_line(line, line_no)
        if tokens:
            inst = parse_line(tokens, line_no)
            insts.append(inst)
    return insts


def allocate_label_numbers(insts):
    label_nr = 0
    label_to_nr = {}
    label_to_nr['exit'] = -1
    for inst in insts:
        if inst[1] in label_to_nr:
            raise ParseError(inst[0], "Label already defined")
        label_to_nr[inst[1]] = label_nr
        label_nr += 1
    return label_to_nr, label_nr - 1


def validate_labels(insts, label_to_nr):
    for inst in insts:
        if inst[2] == 'movdbz':
            if inst[5] not in label_to_nr:
                raise ParseError(inst[0], "Unknown label " + inst[5])
            if inst[6] not in label_to_nr:
                raise ParseError(inst[0], "Unknown label " + inst[6])


def get_reg_nr(reg):
    if reg[0] == 'r':
        return int(reg[1:])
    else:
        return -1


def find_max_reg_nr(insts):
    max_reg_nr = -1
    for inst in insts:
        if inst[2] == 'movdbz':
            max_reg_nr = max(max_reg_nr, get_reg_nr(inst[3]))
            max_reg_nr = max(max_reg_nr, get_reg_nr(inst[4]))
    return max_reg_nr


def get_int(string):
    if string[0:2] == '0x':
        return int(string, 16)
    else:
        return int(string)


def find_consts(insts):
    consts = set()
    for inst in insts:
        if inst[2] == 'movdbz':
            if inst[4] != 'discard' and inst[4][0] != 'r':
                consts.add(get_int(inst[4]))
    return consts


def allocate_consts(consts, max_reg_nr):
    consts = list(consts)
    consts.sort()
    max_const_reg_nr = max_reg_nr
    const_to_reg_nr = {}
    for const in consts:
        max_const_reg_nr += 1
        const_to_reg_nr[const] = max_const_reg_nr
    return max_const_reg_nr, const_to_reg_nr


def write_reg(stream, reg, const_to_reg_nr):
    if reg[0] == 'r':
        stream.write(str(get_reg_nr(reg)))
    elif reg == 'discard':
        stream.write("-1")
    else:
        stream.write(str(const_to_reg_nr[get_int(reg)]))


def write_inst(stream, inst, const_to_reg_nr, label_to_nr):
    stream.write("  gen_movdbz(" + str(label_to_nr[inst[1]]) + ", ")
    write_reg(stream, inst[3], const_to_reg_nr)
    stream.write(", ")
    write_reg(stream, inst[4], const_to_reg_nr)
    stream.write(", " + str(label_to_nr[inst[5]]) + ", " +
                 str(label_to_nr[inst[6]]) + ");")
    stream.write("\t// " + inst[1] + ": movdbz " +
                 inst[3] + ", " + inst[4] + ", " +
                 inst[5] + ", " + inst[6] + "\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('filename', help='intput file name')
    parser.add_argument('-o', help='output file name', metavar='filename')

    args = parser.parse_args()
    if args.o:
        output_file_name = args.o
    else:
        name, ext = os.path.splitext(args.filename)
        if ext == '.s':
            output_file_name = name + '.c'
        else:
            output_file_name = args.filename + '.c'
        output_file_name = os.path.basename(output_file_name)

    try:
        with open(args.filename, 'rb') as stream:
            insts = read_file(stream)
            label_to_nr, max_label_nr = allocate_label_numbers(insts)
            validate_labels(insts, label_to_nr)
            max_reg_nr = find_max_reg_nr(insts)
            consts = find_consts(insts)
            max_const_reg_nr, const_to_reg_nr = allocate_consts(consts, max_reg_nr)
    except ParseError as err:
        sys.stderr.write(os.path.basename(args.filename) + ":" +
                         str(err.line_no) + ": " + err.value + '\n')
        sys.exit(1)

    with open(output_file_name, 'w') as stream:
        stream.write("// Generated by 'movdbz-as.py " + args.filename +
                     "'\n\n")
        stream.write("#define FIRST_INST_PAGE " +
                     "(REG_R0_PAGE + " + str(max_const_reg_nr + 1) + ")\n")
        stream.write("#define LAST_INST_PAGE (FIRST_INST_PAGE + " +
                     str(max_label_nr + 1) +
                     " * (PAGES_PER_INST) - 1)\n")
        stream.write("\n")
        stream.write("static void\n")
        stream.write("gen_program(void)\n")
        stream.write("{\n")
        stream.write("  // Registers\n")
        for i in range(max_reg_nr + 1):
            stream.write("  gen_reg(" + str(i) + ", 0);\n")
        stream.write("\n")
        if consts:
            stream.write("  // Constants\n")
            for const in consts:
                stream.write("  gen_reg(" +
                             str(const_to_reg_nr[const]) + ", " +
                             str(const) + ");\n")
            stream.write("\n")
        stream.write("  // Program\n")

        for inst in insts:
            write_inst(stream, inst, const_to_reg_nr, label_to_nr)

        stream.write("}\n")

if __name__ == '__main__':
    main()
