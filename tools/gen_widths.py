#!/usr/bin/env python3
"""Gera src/pdf_widths.h a partir dos AFM das fontes base Helvetica."""
import re, os

AFM_DIR = "/usr/local/lib/python2.7/site-packages/matplotlib/mpl-data/fonts/pdfcorefonts"

WINANSI = {}
for c in range(32, 127):
    WINANSI[c] = None  # preenchido abaixo
ASCII_NAMES = {
    32:'space',33:'exclam',34:'quotedbl',35:'numbersign',36:'dollar',37:'percent',
    38:'ampersand',39:'quotesingle',40:'parenleft',41:'parenright',42:'asterisk',
    43:'plus',44:'comma',45:'hyphen',46:'period',47:'slash',
    58:'colon',59:'semicolon',60:'less',61:'equal',62:'greater',63:'question',64:'at',
    91:'bracketleft',92:'backslash',93:'bracketright',94:'asciicircum',95:'underscore',
    96:'grave',123:'braceleft',124:'bar',125:'braceright',126:'asciitilde',
}
DIGITS = ['zero','one','two','three','four','five','six','seven','eight','nine']
for i, n in enumerate(DIGITS):
    ASCII_NAMES[48 + i] = n
for c in range(65, 91):
    ASCII_NAMES[c] = chr(c)
for c in range(97, 123):
    ASCII_NAMES[c] = chr(c)
WINANSI.update(ASCII_NAMES)

HIGH = {
    127:'bullet',128:'Euro',129:'bullet',130:'quotesinglbase',131:'florin',
    132:'quotedblbase',133:'ellipsis',134:'dagger',135:'daggerdbl',136:'circumflex',
    137:'perthousand',138:'Scaron',139:'guilsinglleft',140:'OE',141:'bullet',
    142:'Zcaron',143:'bullet',144:'bullet',145:'quoteleft',146:'quoteright',
    147:'quotedblleft',148:'quotedblright',149:'bullet',150:'endash',151:'emdash',
    152:'tilde',153:'trademark',154:'scaron',155:'guilsinglright',156:'oe',
    157:'bullet',158:'zcaron',159:'Ydieresis',160:'space',161:'exclamdown',
    162:'cent',163:'sterling',164:'currency',165:'yen',166:'brokenbar',167:'section',
    168:'dieresis',169:'copyright',170:'ordfeminine',171:'guillemotleft',
    172:'logicalnot',173:'hyphen',174:'registered',175:'macron',176:'degree',
    177:'plusminus',178:'twosuperior',179:'threesuperior',180:'acute',181:'mu',
    182:'paragraph',183:'periodcentered',184:'cedilla',185:'onesuperior',
    186:'ordmasculine',187:'guillemotright',188:'onequarter',189:'onehalf',
    190:'threequarters',191:'questiondown',192:'Agrave',193:'Aacute',
    194:'Acircumflex',195:'Atilde',196:'Adieresis',197:'Aring',198:'AE',
    199:'Ccedilla',200:'Egrave',201:'Eacute',202:'Ecircumflex',203:'Edieresis',
    204:'Igrave',205:'Iacute',206:'Icircumflex',207:'Idieresis',208:'Eth',
    209:'Ntilde',210:'Ograve',211:'Oacute',212:'Ocircumflex',213:'Otilde',
    214:'Odieresis',215:'multiply',216:'Oslash',217:'Ugrave',218:'Uacute',
    219:'Ucircumflex',220:'Udieresis',221:'Yacute',222:'Thorn',223:'germandbls',
    224:'agrave',225:'aacute',226:'acircumflex',227:'atilde',228:'adieresis',
    229:'aring',230:'ae',231:'ccedilla',232:'egrave',233:'eacute',234:'ecircumflex',
    235:'edieresis',236:'igrave',237:'iacute',238:'icircumflex',239:'idieresis',
    240:'eth',241:'ntilde',242:'ograve',243:'oacute',244:'ocircumflex',245:'otilde',
    246:'odieresis',247:'divide',248:'oslash',249:'ugrave',250:'uacute',
    251:'ucircumflex',252:'udieresis',253:'yacute',254:'thorn',255:'ydieresis',
}
WINANSI.update(HIGH)


def load_afm(path):
    widths = {}
    with open(path, encoding='latin-1') as fh:
        for line in fh:
            m = re.match(r'C (-?\d+) ; WX (-?\d+) ; N (\S+)', line)
            if m:
                widths[m.group(3)] = int(m.group(2))
    return widths


def table(widths, name):
    out = ["static const short %s[224] = {" % name]
    row = []
    missing = set()
    for c in range(32, 256):
        glyph = WINANSI.get(c)
        w = widths.get(glyph)
        if w is None:
            missing.add((c, glyph))
            w = widths.get('space', 278)
        row.append("%4d," % w)
        if len(row) == 12:
            out.append("    " + " ".join(row))
            row = []
    if row:
        out.append("    " + " ".join(row))
    out.append("};")
    if missing:
        print("  aviso, glifos ausentes:", sorted(missing))
    return "\n".join(out)


reg = load_afm(os.path.join(AFM_DIR, 'Helvetica.afm'))
bold = load_afm(os.path.join(AFM_DIR, 'Helvetica-Bold.afm'))

body = """/* pdf_widths.h — larguras dos glifos (em 1/1000 de em) das fontes base
   Helvetica e Helvetica-Bold, indexadas pelo código WinAnsi menos 32.
   GERADO por tools/gen_widths.py — não editar à mão. */
#ifndef RAQ_PDF_WIDTHS_H
#define RAQ_PDF_WIDTHS_H

%s

%s

#endif /* RAQ_PDF_WIDTHS_H */
""" % (table(reg, 'largura_helvetica'), table(bold, 'largura_helvetica_bold'))

dest = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'src', 'pdf_widths.h')
with open(dest, 'w') as fh:
    fh.write(body)
print("gerado:", dest)
