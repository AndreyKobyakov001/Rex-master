#!/usr/bin/env python

import sys
import re
import io
import functools

def convert_file(inFname, outFname):
    inFile = open(inFname, "r", encoding="utf-8")
    outFile = open(outFname, "w")
    contents = inFile.read()
    inFile.close()
    # Strip off all the schema info from the TA file.
    start = contents.find("FACT TUPLE :")+len("FACT TUPLE :")
    contents = contents[start:]
    base = '^\$INSTANCE[ \t]+([a-zA-Z0-9]+|"[[_ \/.a-zA-Z0-9():&>\-]+")[ \t]+([a-zA-Z0-9]+)$'
    pat = re.compile(base,flags=re.MULTILINE)
    entities = pat.findall(contents)
    #entities = list(map(lambda x: tuple(filter(lambda y: y!='', x)), entities))
    print(entities)
    for i in entities:
        entityLine = "MERGE (" + ":" + i[1] +" {" + "id:\"" + i[0]+"\"" + " "
        attribsPat = re.escape(i[0]) + '[ \t]+{ (.*) }'
        attribString = re.compile(attribsPat).findall(contents)
        attPat = '([a-zA-Z0-9]+) = ([_a-zA-Z0-9:&>\-]+|"[_ \/.a-zA-Z0-9():&>\-]+"|\([ ._\/a-zA-Z0-9:&>\-]+\))'
        attList = re.compile(attPat).findall(attribString[0])
        for x in range(len(attList)):
            att = attList[x]
            attName = att[0].strip()
            value = ""
            if (attName == "time"):
                value = "datetime('"+att[1].strip().strip('"') + "')"
            elif att[1].strip().strip('"').isnumeric():
                value = att[1].strip().strip('"')
            else:
                value = att[1].strip()
                if value[0] != '"':
                    value = '"' + value + '"'
            entityLine += ", " + attName + ":" + value
        entityLine += "});\n\n"
        outFile.write(entityLine)
    # Find all relationships NOT in parentheses
    base = '^([a-zA-Z0-9]+) ([a-zA-Z0-9]+|"[[_ \/.a-zA-Z0-9():&>\-]+") ([a-zA-Z0-9]+|"[[_ \/.a-zA-Z0-9():&>\-]+") ?$'
    pat = re.compile(base, re.M)
    rels = pat.findall(contents)
    rels = list(filter(lambda x : x[0] != "INSTANCE", rels))
    relAttSuff = '{ [a-zA-Z0-9]+ = "[a-zA-Z0-9]"'
    for i in rels:
        relLine = "MATCH (a {id:\"" + i[1] + "\"}), (b {id:\"" + i[2] + "\"})\n"
        relLine += "  MERGE "
        if i[1] == "Subscriber":
            print(i)
        escaped = list(map(lambda x : re.escape(x), i))
        attribsPat = '\('+ escaped[0] + " " + escaped[1] + " " + escaped[2] + "\)" + '[ \t]+{ (.*) }'
        attribString = re.compile(attribsPat).findall(contents)
        attPat = '([a-zA-Z0-9]+) = ([_a-zA-Z0-9:&>\-]+|"[_ \/.a-zA-Z0-9():&|=!<>\-]+"|\([_ \/.a-zA-Z0-9():&|=!<>\-]+\))'
        attList = []
        if (len(attribString)):
            attList = re.compile(attPat).findall(attribString[0])
        relLine += "(" + "a" + ")-[:" + i[0].upper() +" "
        if (len(attList)):
            relLine += "{"
        for x in range(len(attList)):
            att = attList[x]
            attName = att[0].strip()
            value = ""
            if (attName == "time"):
                value = "datetime('"+att[1].strip().strip('"') + "')"
            elif att[1].strip().strip('"').isnumeric():
                value = att[1].strip().strip('"')
            else:
                value = att[1].strip()
                if value[0] != '"':
                    value = '"' + value + '"'
            relLine += attName + ":" + value
            if x != len(attList)-1:
                relLine += ", "
            else:
                entityLine += " "
        if (len(attList)):
            relLine += "} "
        relLine += "]->("+"b"+");\n\n"
        outFile.write(relLine)
    # TODO Find relationships with attributes.
    outFile.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: " + sys.argv[0] + " taModelPath [outputPath]")
        exit(1)
    inFname = sys.argv[1]
    outFname = sys.argv[1][:inFname.rfind(".")] + ".cypher"
    if (inFname == outFname):
        outFname += "2"
    if len(sys.argv) >= 3:
        outFname = sys.argv[2]
    print("Converting, this will take some time...")
    convert_file(inFname, outFname)
    print("Done converting! - Your CREATE statements are in file " + outFname)
