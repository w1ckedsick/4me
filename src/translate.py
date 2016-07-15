import sys
outf = open(sys.argv[3], 'ab')

if sys.argv[1] == 'size':
    size = 0
    for line in open(sys.argv[2]):
        if line.strip() == '':
            continue
        if line.strip()[0] == '#':
            continue
        size += 4
    if size >= 2**16:
        raise ValueError("Code size is too big")
    ba = bytearray([size >> 8, size & 0xff])
    outf.write(ba)
    exit()


opcodes = {'ADD': 0x0, 'SUB': 0x1, 'MUL': 0x2, 'MODU': 0x3, 'DIV': 0x4, 'DIVU': 0x5, 'ORNOT': 0x6, 'AND': 0x7, 'LSL': 0x8, 'LSR': 0x9, 'ASR': 0xa, 'CMP': 0xb, 'BRN': 0xc, 'LD': 0xd, 'ST': 0xe}
comps = {'EQ': 0x0, 'NE': 0x1, 'BN': 0x2, 'BS': 0x3, 'LS': 0x4, 'GT': 0x5, 'GE': 0x6, 'LE': 0x7, 'BL': 0x8, 'AB': 0x9, 'BE': 0xa, 'AE': 0xb}
for line in open(sys.argv[2]):
    if line.strip() == '':
        continue
    if line.strip()[0] == '#':
        continue

    args = line.split()
    args = [x.strip(',') for x in args]

    opc = opcodes[args[0].upper()]
    if (args[1].upper()[0], args[2].upper()[0], args[3].upper()[0]) != ('R', 'R', 'R'):
        raise ValueError("registers shall be named started with R/r")
    else:
        rd = int(args[1][1:], 0)
        rs1 = int(args[2][1:], 0)
        rs2 = int(args[3][1:], 0)

    imm = comps[args[4]] if args[4] in comps else int(args[4],0)

    #print opc, rd, rs1, rs2, imm
    ba = bytearray([opc << 4 | rd, rs1 << 4 | rs2, imm >> 8, imm & 0xff])
    #print 
    outf.write(ba)

outf.close()
