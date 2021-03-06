/*
 * Cppcheck - A tool for static C/C++ code analysis
 * Copyright (C) 2007-2010 Daniel Marjamäki and Cppcheck team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


//---------------------------------------------------------------------------
#ifdef _MSC_VER
#pragma warning(disable: 4503)
#endif

#include "tokenize.h"
#include "token.h"
#include "filelister.h"
#include "mathlib.h"
#include "settings.h"
#include "errorlogger.h"
#include "check.h"

#include <locale>
#include <fstream>
#include <string>
#include <cstring>
#include <iostream>
#include <sstream>
#include <list>
#include <cassert>
#include <algorithm>
#include <cctype>
#include <stack>
#include <stdexcept>    // for std::runtime_error

//---------------------------------------------------------------------------

Tokenizer::Tokenizer()
    : _settings(0), _errorLogger(0)
{
    _tokens = 0;
    _tokensBack = 0;
}

Tokenizer::Tokenizer(const Settings *settings, ErrorLogger *errorLogger)
    : _settings(settings), _errorLogger(errorLogger)
{
    _tokens = 0;
    _tokensBack = 0;
}

Tokenizer::~Tokenizer()
{
    deallocateTokens();
}

//---------------------------------------------------------------------------

// Helper functions..


//---------------------------------------------------------------------------

const Token *Tokenizer::tokens() const
{
    return _tokens;
}


const std::vector<std::string> *Tokenizer::getFiles() const
{
    return &_files;
}

//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// addtoken
// add a token. Used by 'Tokenizer'
//---------------------------------------------------------------------------

void Tokenizer::addtoken(const char str[], const unsigned int lineno, const unsigned int fileno, bool split)
{
    if (str[0] == 0)
        return;

    // If token contains # characters, split it up
    if (split && strstr(str, "##"))
    {
        std::string temp;
        for (unsigned int i = 0; str[i]; ++i)
        {
            if (strncmp(&str[i], "##", 2) == 0)
            {
                addtoken(temp.c_str(), lineno, fileno, false);
                temp.clear();
                addtoken("##", lineno, fileno, false);
                ++i;
            }
            else
                temp += str[i];
        }
        addtoken(temp.c_str(), lineno, fileno, false);
        return;
    }

    // Replace hexadecimal value with decimal
    std::ostringstream str2;
    if (strncmp(str, "0x", 2) == 0)
    {
        str2 << std::strtoul(str + 2, NULL, 16);
    }
    else
    {
        str2 << str;
    }

    if (_tokensBack)
    {
        _tokensBack->insertToken(str2.str().c_str());
    }
    else
    {
        _tokens = new Token(&_tokensBack);
        _tokensBack = _tokens;
        _tokensBack->str(str2.str());
    }

    _tokensBack->linenr(lineno);
    _tokensBack->fileIndex(fileno);
}

void Tokenizer::addtoken(const Token * tok, const unsigned int lineno, const unsigned int fileno)
{
    if (tok == 0)
        return;

    // Replace hexadecimal value with decimal
    std::ostringstream str2;
    if (strncmp(tok->str().c_str(), "0x", 2) == 0)
    {
        str2 << std::strtoul(tok->str().c_str() + 2, NULL, 16);
    }
    else
    {
        str2 << tok->str();
    }

    if (_tokensBack)
    {
        _tokensBack->insertToken(str2.str().c_str());
    }
    else
    {
        _tokens = new Token(&_tokensBack);
        _tokensBack = _tokens;
        _tokensBack->str(str2.str());
    }

    _tokensBack->linenr(lineno);
    _tokensBack->fileIndex(fileno);
    _tokensBack->isUnsigned(tok->isUnsigned());
    _tokensBack->isSigned(tok->isSigned());
    _tokensBack->isLong(tok->isLong());
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// SizeOfType - gives the size of a type
//---------------------------------------------------------------------------



unsigned int Tokenizer::sizeOfType(const Token *type) const
{
    if (!type || type->str().empty())
        return 0;

    if (type->str()[0] == '"')
        return static_cast<unsigned int>(Token::getStrLength(type) + 1);

    std::map<std::string, unsigned int>::const_iterator it = _typeSize.find(type->strAt(0));
    if (it == _typeSize.end())
        return 0;
    else if (type->isLong())
    {
        if (type->str() == "double")
            return sizeof(long double);
        else if (type->str() == "long")
            return sizeof(long long);
    }

    return it->second;
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// InsertTokens - Copy and insert tokens
//---------------------------------------------------------------------------

void Tokenizer::insertTokens(Token *dest, const Token *src, unsigned int n)
{
    while (n > 0)
    {
        dest->insertToken(src->str().c_str());
        dest = dest->next();
        dest->fileIndex(src->fileIndex());
        dest->linenr(src->linenr());
        dest->varId(src->varId());
        dest->isUnsigned(src->isUnsigned());
        dest->isSigned(src->isSigned());
        dest->isLong(src->isLong());
        src  = src->next();
        --n;
    }
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Tokenize - tokenizes a given file.
//---------------------------------------------------------------------------

void Tokenizer::createTokens(std::istream &code)
{
    // line number in parsed code
    unsigned int lineno = 1;

    // The current token being parsed
    std::string CurrentToken;

    // lineNumbers holds line numbers for files in fileIndexes
    // every time an include file is complitely parsed, last item in the vector
    // is removed and lineno is set to point to that value.
    std::vector<unsigned int> lineNumbers;

    // fileIndexes holds index for _files vector about currently parsed files
    // every time an include file is complitely parsed, last item in the vector
    // is removed and FileIndex is set to point to that value.
    std::vector<unsigned int> fileIndexes;

    // FileIndex. What file in the _files vector is read now?
    unsigned int FileIndex = 0;

    // Read one byte at a time from code and create tokens
    for (char ch = (char)code.get(); code.good(); ch = (char)code.get())
    {
        // char/string..
        // multiline strings are not handled. The preprocessor should handle that for us.
        if (ch == '\'' || ch == '\"')
        {
            std::string line;

            // read char
            bool special = false;
            char c = ch;
            do
            {
                // Append token..
                line += c;

                // Special sequence '\.'
                if (special)
                    special = false;
                else
                    special = (c == '\\');

                // Get next character
                c = (char)code.get();
            }
            while (code.good() && (special || c != ch));
            line += ch;

            // Handle #file "file.h"
            if (CurrentToken == "#file")
            {
                // Extract the filename
                line = line.substr(1, line.length() - 2);

                // Has this file been tokenized already?
                ++lineno;
                bool foundOurfile = false;
                fileIndexes.push_back(FileIndex);
                for (unsigned int i = 0; i < _files.size(); i++)
                {
                    if (getFileLister()->sameFileName(_files[i].c_str(), line.c_str()))
                    {
                        // Use this index
                        foundOurfile = true;
                        FileIndex = i;
                    }
                }

                if (!foundOurfile)
                {
                    // The "_files" vector remembers what files have been tokenized..
                    _files.push_back(getFileLister()->simplifyPath(line.c_str()));
                    FileIndex = static_cast<unsigned int>(_files.size() - 1);
                }

                lineNumbers.push_back(lineno);
                lineno = 0;
            }
            else
            {
                // Add previous token
                addtoken(CurrentToken.c_str(), lineno, FileIndex);

                // Add content of the string
                addtoken(line.c_str(), lineno, FileIndex);
            }

            CurrentToken.clear();

            continue;
        }

        if (strchr("+-*/%&|^?!=<>[](){};:,.~\n ", ch))
        {
            if (ch == '.' &&
                CurrentToken.length() > 0 &&
                std::isdigit(CurrentToken[0]))
            {
                // Don't separate doubles "5.4"
            }
            else if (strchr("+-", ch) &&
                     CurrentToken.length() > 0 &&
                     std::isdigit(CurrentToken[0]) &&
                     (CurrentToken[CurrentToken.length()-1] == 'e' ||
                      CurrentToken[CurrentToken.length()-1] == 'E'))
            {
                // Don't separate doubles "4.2e+10"
            }
            else
            {
                if (CurrentToken == "#file")
                {
                    // Handle this where strings are handled
                    continue;
                }
                else if (CurrentToken == "#endfile")
                {
                    if (lineNumbers.empty() || fileIndexes.empty())
                    {
                        std::cerr << "####### Preprocessor bug! #######\n";
                        std::exit(0);
                    }

                    lineno = lineNumbers.back();
                    lineNumbers.pop_back();
                    FileIndex = fileIndexes.back();
                    fileIndexes.pop_back();
                    CurrentToken.clear();
                    continue;
                }

                addtoken(CurrentToken.c_str(), lineno, FileIndex, true);

                CurrentToken.clear();

                if (ch == '\n')
                {
                    ++lineno;
                    continue;
                }
                else if (ch == ' ')
                {
                    continue;
                }

                CurrentToken += ch;
                // Add "++", "--" or ">>" token
                if ((ch == '+' || ch == '-' || ch == '>') && (code.peek() == ch))
                    CurrentToken += (char)code.get();
                addtoken(CurrentToken.c_str(), lineno, FileIndex);
                CurrentToken.clear();
                continue;
            }
        }

        CurrentToken += ch;
    }
    addtoken(CurrentToken.c_str(), lineno, FileIndex, true);
}

void Tokenizer::duplicateTypedefError(const Token *tok1, const Token *tok2, const std::string &type)
{
    if (!(_settings && _settings->_checkCodingStyle))
        return;

    std::list<ErrorLogger::ErrorMessage::FileLocation> locationList;
    ErrorLogger::ErrorMessage::FileLocation loc;
    loc.line = tok1->linenr();
    loc.file = file(tok1);
    locationList.push_back(loc);
    loc.line = tok2->linenr();
    loc.file = file(tok2);
    locationList.push_back(loc);

    const ErrorLogger::ErrorMessage errmsg(locationList,
                                           "style",
                                           std::string(type + " '" + tok2->str() +
                                                   "' hides typedef with same name"),
                                           "variableHidingTypedef");

    if (_errorLogger)
        _errorLogger->reportErr(errmsg);
    else
        Check::reportError(errmsg);
}

void Tokenizer::duplicateDeclarationError(const Token *tok1, const Token *tok2, const std::string &type)
{
    if (!(_settings && _settings->_checkCodingStyle))
        return;

    std::list<ErrorLogger::ErrorMessage::FileLocation> locationList;
    ErrorLogger::ErrorMessage::FileLocation loc;
    loc.line = tok1->linenr();
    loc.file = file(tok1);
    locationList.push_back(loc);
    loc.line = tok2->linenr();
    loc.file = file(tok2);
    locationList.push_back(loc);

    const ErrorLogger::ErrorMessage errmsg(locationList,
                                           "style",
                                           std::string(type + " '" + tok2->str() +
                                                   "' forward declaration unnecessary, already declared"),
                                           "unnecessaryForwardDeclaration");

    if (_errorLogger)
        _errorLogger->reportErr(errmsg);
    else
        Check::reportError(errmsg);
}

// check if this statement is a duplicate definition
bool Tokenizer::duplicateTypedef(Token **tokPtr, const Token *name)
{
    // check for an end of definition
    const Token * tok = *tokPtr;
    if (tok && tok->next() && Token::Match(tok->next(), ";|,|[|=|)|>|(|{"))
    {
        const Token * end = tok->next();

        if (end->str() == "[")
        {
            end = end->link()->next();
        }
        else if (end->str() == ",")
        {
            // check for derived class
            if (Token::Match(tok->previous(), "public|private|protected"))
                return false;

            // find end of definition
            int level = 0;
            while (end && end->next() && (!Token::Match(end->next(), ";|)|>") ||
                                          (end->next()->str() == ")" && level == 0)))
            {
                if (end->next()->str() == "(")
                    level++;
                else if (end->next()->str() == ")")
                    level--;

                end = end->next();
            }
            end = end->next();
        }
        else if (end->str() == "(")
        {
            if (tok->previous()->str() == "operator")
            {
                // conversion operator
                return false;
            }
            else if (tok->previous()->str() == "typedef")
            {
                // typedef of function returning this type
                return false;
            }
            else if (Token::Match(tok->previous(), "public:|private:|protected:"))
            {
                return false;
            }
            else if (tok->previous()->str() == ">")
            {
                if (!Token::Match(tok->tokAt(-2), "%type%"))
                    return false;

                duplicateTypedefError(*tokPtr, name, "Template instantiation");
                *tokPtr = end->link();
                return true;
            }
            else if (Token::Match(tok->previous(), "%type%"))
            {
                if (end->link()->next()->str() == "{")
                {
                    duplicateTypedefError(*tokPtr, name, "Function");
                    *tokPtr = end->link()->next()->link();
                    return true;
                }
            }
        }

        if (end)
        {
            if (Token::Match(end, ") {")) // function parameter ?
            {
                // look backwards
                if (Token::Match(tok->previous(), "%type%") &&
                    !Token::Match(tok->previous(), "return|new|const"))
                {
                    duplicateTypedefError(*tokPtr, name, "Function parameter");
                    // duplicate definition so skip entire function
                    *tokPtr = end->next()->link();
                    return true;
                }
            }
            else if (end->str() == ">") // template parameter ?
            {
                // look backwards
                if (Token::Match(tok->previous(), "%type%") &&
                    !Token::Match(tok->previous(), "return|new|const|volatile"))
                {
                    // duplicate definition so skip entire template
                    while (end && end->str() != "{")
                        end = end->next();
                    if (end)
                    {
                        duplicateTypedefError(*tokPtr, name, "Template parameter");
                        *tokPtr = end->link();
                        return true;
                    }
                }
            }
            else
            {
                // look backwards
                if (Token::Match(tok->previous(), "typedef|}|>") ||
                    (Token::Match(tok->previous(), "%type%") &&
                     (!Token::Match(tok->previous(), "return|new|const|friend|public|private|protected") &&
                      !Token::Match(tok->tokAt(-2), "friend class"))))
                {
                    // scan backwards for the end of the previous statement
                    int level = (tok->previous()->str() == "}") ? 1 : 0;
                    while (tok && tok->previous() && (!Token::Match(tok->previous(), ";|{") || (level != 0)))
                    {
                        if (tok->previous()->str() == "typedef")
                        {
                            duplicateTypedefError(*tokPtr, name, "Typedef");
                            return true;
                        }
                        else if (tok->previous()->str() == "enum")
                        {
                            duplicateTypedefError(*tokPtr, name, "Enum");
                            return true;
                        }
                        else if (tok->previous()->str() == "struct")
                        {
                            if (tok->next()->str() != ";")
                            {
                                duplicateTypedefError(*tokPtr, name, "Struct");
                                return true;
                            }
                            else
                            {
                                // forward declaration after declaration
                                duplicateDeclarationError(*tokPtr, name, "Struct");
                                return false;
                            }
                        }
                        else if (tok->previous()->str() == "union")
                        {
                            if (tok->next()->str() != ";")
                            {
                                duplicateTypedefError(*tokPtr, name, "Union");
                                return true;
                            }
                            else
                            {
                                // forward declaration after declaration
                                duplicateDeclarationError(*tokPtr, name, "Union");
                                return false;
                            }
                        }
                        else if (tok->previous()->str() == "class")
                        {
                            if (tok->next()->str() != ";")
                            {
                                duplicateTypedefError(*tokPtr, name, "Class");
                                return true;
                            }
                            else
                            {
                                // forward declaration after declaration
                                duplicateDeclarationError(*tokPtr, name, "Class");
                                return false;
                            }
                        }
                        else if (tok->previous()->str() == "{")
                            level--;

                        tok = tok->previous();
                    }

                    duplicateTypedefError(*tokPtr, name, "Variable");
                    return true;
                }
            }
        }
    }

    return false;
}

struct SpaceInfo
{
    bool isNamespace;
    std::string className;
    const Token * classEnd;
};

void Tokenizer::simplifyTypedef()
{
    std::vector<SpaceInfo> spaceInfo;
    bool isNamespace = false;
    std::string className;
    bool hasClass = false;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "class|struct|namespace %any%"))
        {
            isNamespace = (tok->str() == "namespace");
            hasClass = true;
            className = tok->next()->str();
            continue;
        }
        else if (hasClass && tok->str() == ";")
        {
            hasClass = false;
            continue;
        }
        else if (hasClass && tok->str() == "{")
        {
            SpaceInfo info;
            info.isNamespace = isNamespace;
            info.className = className;
            info.classEnd = tok->link();
            spaceInfo.push_back(info);

            hasClass = false;
            continue;
        }
        else if (!spaceInfo.empty() && tok->str() == "}" && spaceInfo.back().classEnd == tok)
        {
            spaceInfo.pop_back();
            continue;
        }
        else if (tok->str() != "typedef")
            continue;

        // pull struct, union, enum or class definition out of typedef
        // use typedef name for unnamed struct, union, enum or class
        if (Token::Match(tok->next(), "struct|enum|union|class %type% {") ||
            Token::Match(tok->next(), "struct|enum|union|class {"))
        {
            Token *tok1;
            std::string name;

            if (tok->tokAt(2)->str() == "{") // unnamed
            {
                tok1 = tok->tokAt(2)->link();

                if (tok1 && tok1->next())
                {
                    // use typedef name if available
                    if (Token::Match(tok1->next(), "%type%"))
                        name = tok1->next()->str();
                    else // create a unique name
                    {
                        static long count = 0;
                        name = "Unnamed" + MathLib::toString<long>(count++);
                    }
                    tok->tokAt(1)->insertToken(name.c_str());
                }
                else
                    continue;
            }
            else // has a name
            {
                tok1 = tok->tokAt(3)->link();

                if (!tok1)
                    continue;

                name = tok->tokAt(2)->str();
            }

            tok1->insertToken(";");
            tok1 = tok1->next();
            tok1->insertToken("typedef");
            tok1 = tok1->next();
            Token * tok3 = tok1;
            tok1->insertToken(tok->next()->strAt(0)); // struct, union or enum
            tok1 = tok1->next();
            tok1->insertToken(name.c_str());
            tok->deleteThis();
            tok = tok3;
        }

        Token *typeName;
        std::list<std::string> pointers;
        Token *typeStart = 0;
        Token *typeEnd = 0;
        Token *argStart = 0;
        Token *argEnd = 0;
        Token *arrayStart = 0;
        Token *arrayEnd = 0;
        Token *typeDef = tok;
        int offset = 1;
        bool functionPtr = false;
        bool functionRef = false;

        if (Token::Match(tok->next(), "::") ||
            Token::Match(tok->next(), "%type%"))
        {
            typeStart = tok->next();
            offset = 1;

            typeEnd = tok->tokAt(offset++);

            bool atEnd = false;
            while (!atEnd)
            {
                if (tok->tokAt(offset) && Token::Match(tok->tokAt(offset), "::"))
                    typeEnd = tok->tokAt(offset++);

                if (tok->tokAt(offset) && Token::Match(tok->tokAt(offset), "%type%") &&
                    tok->tokAt(offset + 1) && !Token::Match(tok->tokAt(offset + 1), "[|;|,"))
                    typeEnd = tok->tokAt(offset++);
                else
                    atEnd = true;
            }
        }
        else
            continue; // invalid input

        // check for template
        if (tok->tokAt(offset)->str() == "<")
        {
            int level = 1;
            int paren = 0;
            typeEnd = tok->tokAt(offset + 1);
            for (; typeEnd ; typeEnd = typeEnd->next())
            {
                if (typeEnd->str() == ">")
                {
                    if (paren == 0)
                    {
                        level--;
                        if (level == 0)
                            break;
                    }
                }
                else if (typeEnd->str() == "<")
                {
                    if (paren == 0)
                        level++;
                }
                else if (typeEnd->str() == "(")
                    paren++;
                else if (typeEnd->str() == ")")
                    paren--;
            }

            while (typeEnd && typeEnd->next() && Token::Match(typeEnd->next(), ":: %type%"))
                typeEnd = typeEnd->tokAt(2);

            if (!typeEnd)
            {
                // internal error
                return;
            }

            tok = typeEnd;
            offset = 1;
        }

        // check for pointers and references
        while (tok->tokAt(offset) && Token::Match(tok->tokAt(offset), "*|&"))
            pointers.push_back(tok->tokAt(offset++)->str());

        if (tok->tokAt(offset) && Token::Match(tok->tokAt(offset), "%type%"))
        {
            // found the type name
            typeName = tok->tokAt(offset++);

            // check for array
            if (tok->tokAt(offset) && tok->tokAt(offset)->str() == "[")
            {
                arrayStart = tok->tokAt(offset);

                bool atEnd = false;
                while (!atEnd)
                {
                    while (tok->tokAt(offset + 1) && !Token::Match(tok->tokAt(offset + 1), ";|,"))
                        offset++;

                    if (!tok->tokAt(offset + 1))
                        return; // invalid input
                    else if (tok->tokAt(offset + 1)->str() == ";")
                        atEnd = true;
                    else if (tok->tokAt(offset)->str() == "]")
                        atEnd = true;
                    else
                        offset++;
                }

                arrayEnd = tok->tokAt(offset++);
            }

            // check for end or another
            if (tok->tokAt(offset) && Token::Match(tok->tokAt(offset), ";|,"))
                tok = tok->tokAt(offset);
            else
            {
                // unhandled typedef, skip it and continue
                tok = tok->tokAt(offset);
                continue;
            }
        }
        else if (tok->tokAt(offset) && Token::Match(tok->tokAt(offset), "( *|& %type% ) ("))
        {
            if (tok->tokAt(offset + 4)->link()->next())
            {
                functionPtr = tok->tokAt(offset + 1)->str() == "*";
                functionRef = tok->tokAt(offset + 1)->str() == "&";
                typeName = tok->tokAt(offset + 2);
                argStart = tok->tokAt(offset + 4);
                argEnd = tok->tokAt(offset + 4)->link();
                tok = argEnd->next();
            }
            else
            {
                // internal error
                continue;
            }
        }
        else
        {
            // unhandled typedef, skip it and continue
            continue;
        }

        bool done = false;
        bool ok = true;

        while (!done)
        {
            std::string pattern;
            int scope = 0;
            bool inScope = true;

            bool exitThisScope = false;
            int exitScope = 0;
            bool simplifyType = false;
            unsigned int classLevel = spaceInfo.size();

            for (unsigned int i = classLevel; i < spaceInfo.size(); i++)
                pattern += (spaceInfo[i].className + " :: ");

            pattern += typeName->str();

            for (Token *tok2 = tok; tok2; tok2 = tok2->next())
            {
                if (tok2->str() == "}")
                {
                    if (classLevel > 0 && tok2 == spaceInfo[classLevel - 1].classEnd)
                    {
                        --classLevel;
                        pattern.clear();

                        for (unsigned int i = classLevel; i < spaceInfo.size(); i++)
                            pattern += (spaceInfo[i].className + " :: ");

                        pattern += typeName->str();
                    }
                    else
                    {
                        scope--;
                        if (scope < 0)
                            inScope = false;

                        if (exitThisScope)
                        {
                            if (scope < exitScope)
                                exitThisScope = false;
                        }
                    }
                }
                else if (tok2->str() == "{")
                {
                    scope++;
                }
                else if (Token::Match(tok2, pattern.c_str()))
                {
                    if (pattern != typeName->str()) // has a "something ::"
                    {
                        for (unsigned int i = classLevel; i < spaceInfo.size(); i++)
                        {
                            tok2->deleteNext();
                            tok2->deleteNext();
                        }
                        simplifyType = true;
                    }
                    else if (inScope && !exitThisScope)
                    {
                        if (Token::simpleMatch(tok2->previous(), "::"))
                        {
                            // Don't replace this typename if it's preceded by "::" unless it's a namespace
                            if (!spaceInfo.empty() && (tok2->tokAt(-2)->str() == spaceInfo[0].className) && spaceInfo[0].isNamespace)
                            {
                                tok2 = tok2->tokAt(-3);
                                tok2->deleteNext();
                                tok2->deleteNext();
                                tok2 = tok2->next();
                                simplifyType = true;
                            }
                        }
                        else if (duplicateTypedef(&tok2, typeName))
                        {
                            exitScope = scope;

                            // skip to end of scope if not already there
                            if (tok2->str() != "}")
                            {
                                int level = 0;
                                while (tok2->next() && (tok2->next()->str() != "}" || level))
                                {
                                    if (tok2->next()->str() == "{")
                                        level++;
                                    else if (tok2->next()->str() == "}")
                                        level--;

                                    tok2 = tok2->next();
                                }
                            }
                        }
                        else
                        {
                            simplifyType = true;
                        }
                    }
                }

                if (simplifyType)
                {
                    bool isDerived = false;
                    isDerived = Token::Match(tok2->previous(), "public|protected|private %type% {|,");

                    bool inCast = false;

                    if ((tok2->previous()->str() == "(" && tok2->next()->str() == ")") ||
                        (Token::Match(tok2->tokAt(-1), "<") &&
                         Token::Match(tok2->next(), "> (")))
                        inCast = true;

                    // skip over class or struct in derived class declaration
                    if (isDerived && Token::Match(typeStart, "class|struct"))
                        typeStart = typeStart->next();

                    tok2->str(typeStart->str());
                    Token * nextToken;
                    std::stack<Token *> links;
                    for (nextToken = typeStart->next(); nextToken != typeEnd->next(); nextToken = nextToken->next())
                    {
                        tok2->insertToken(nextToken->strAt(0));
                        tok2 = tok2->next();

                        // Check for links and fix them up
                        if (tok2->str() == "(" || tok2->str() == "[")
                            links.push(tok2);
                        if (tok2->str() == ")" || tok2->str() == "]")
                        {
                            Token * link = links.top();

                            tok2->link(link);
                            link->link(tok2);

                            links.pop();
                        }
                    }

                    while (!pointers.empty())
                    {
                        tok2->insertToken(pointers.front().c_str());
                        pointers.pop_front();
                        tok2 = tok2->next();
                    }

                    if (functionPtr || functionRef)
                    {
                        tok2->insertToken("(");
                        tok2 = tok2->next();
                        Token *tok3 = tok2;
                        if (functionPtr)
                            tok2->insertToken("*");
                        else
                            tok2->insertToken("&");
                        tok2 = tok2->next();

                        if (!inCast)
                        {
                            if (tok2->next()->str() != ")" && tok2->next()->str() != ",")
                            {
                                if (Token::Match(tok2->next(), "( * %type% ) ("))
                                    tok2 = tok2->tokAt(5)->link();
                                else
                                {
                                    if (tok2->next()->str() == "(")
                                        tok2 = tok2->next()->link();
                                    else if (!Token::Match(tok2->next(), "[|>"))
                                    {
                                        tok2 = tok2->next();

                                        while (Token::Match(tok2, "*|&") && tok2->next()->str() != ")")
                                            tok2 = tok2->next();

                                        // skip over typedef parameter
                                        if (tok2->next()->str() == "(")
                                        {
                                            tok2 = tok2->next()->link();

                                            if (tok2->next()->str() == "(")
                                                tok2 = tok2->next()->link();
                                        }
                                    }
                                }
                            }
                        }

                        tok2->insertToken(")");
                        tok2 = tok2->next();
                        Token::createMutualLinks(tok2, tok3);
                        tok2->insertToken("(");
                        tok2 = tok2->next();
                        tok3 = tok2;
                        Token * nextArgTok;
                        std::stack<Token *> argLinks;
                        for (nextArgTok = argStart->next(); nextArgTok != argEnd; nextArgTok = nextArgTok->next())
                        {
                            tok2->insertToken(nextArgTok->strAt(0));
                            tok2 = tok2->next();

                            // Check for links and fix them up
                            if (tok2->str() == "(" || tok2->str() == "[")
                                argLinks.push(tok2);
                            if (tok2->str() == ")" || tok2->str() == "]")
                            {
                                Token * link = argLinks.top();

                                tok2->link(link);
                                link->link(tok2);

                                argLinks.pop();
                            }
                        }
                        tok2->insertToken(")");
                        tok2 = tok2->next();
                        Token::createMutualLinks(tok2, tok3);
                    }

                    if (arrayStart && arrayEnd)
                    {
                        tok2 = tok2->next();
                        Token * nextArrTok;
                        std::stack<Token *> arrLinks;
                        for (nextArrTok = arrayStart; nextArrTok != arrayEnd->next(); nextArrTok = nextArrTok->next())
                        {
                            tok2->insertToken(nextArrTok->strAt(0));
                            tok2 = tok2->next();

                            // Check for links and fix them up
                            if (tok2->str() == "(" || tok2->str() == "[")
                                arrLinks.push(tok2);
                            if (tok2->str() == ")" || tok2->str() == "]")
                            {
                                Token * link = arrLinks.top();

                                tok2->link(link);
                                link->link(tok2);

                                arrLinks.pop();
                            }
                        }
                        tok2 = tok2->next();
                    }

                    simplifyType = false;
                }
            }

            if (tok->str() == ";")
                done = true;
            else if (tok->str() == ",")
            {
                arrayStart = 0;
                arrayEnd = 0;
                offset = 1;

                while (tok->tokAt(offset) && Token::Match(tok->tokAt(offset), "*|&"))
                    pointers.push_back(tok->tokAt(offset++)->str());

                if (tok->tokAt(offset) && Token::Match(tok->tokAt(offset), "%type%"))
                {
                    typeName = tok->tokAt(offset++);

                    if (tok->tokAt(offset) && tok->tokAt(offset)->str() == "[")
                    {
                        arrayStart = tok->tokAt(offset);

                        bool atEnd = false;
                        while (!atEnd)
                        {
                            while (tok->tokAt(offset + 1) && !Token::Match(tok->tokAt(offset + 1), ";|,"))
                                offset++;

                            if (!tok->tokAt(offset + 1))
                                return; // invalid input
                            else if (tok->tokAt(offset + 1)->str() == ";")
                                atEnd = true;
                            else if (tok->tokAt(offset)->str() == "]")
                                atEnd = true;
                            else
                                offset++;
                        }

                        arrayEnd = tok->tokAt(offset++);
                    }

                    if (tok->tokAt(offset) && Token::Match(tok->tokAt(offset), ";|,"))
                        tok = tok->tokAt(offset);
                    else
                    {
                        // we encountered a typedef we don't support yet so just continue
                        done = true;
                        ok = false;
                    }
                }
                else
                {
                    // we encountered a typedef we don't support yet so just continue
                    done = true;
                    ok = false;
                }
            }
            else
            {
                // something is really wrong (internal error)
                done = true;
                ok = false;
            }
        }

        if (ok)
        {
            // remove typedef but leave ;
            while (typeDef->next() && typeDef->next() != tok)
                typeDef->deleteNext();

            if (typeDef != _tokens)
            {
                tok = typeDef->previous();
                tok->deleteNext();
            }
            else
            {
                _tokens->deleteThis();
                tok = _tokens;
            }
        }
    }
}

bool Tokenizer::tokenize(std::istream &code, const char FileName[], const std::string &configuration)
{
    _configuration = configuration;

    // The "_files" vector remembers what files have been tokenized..
    _files.push_back(getFileLister()->simplifyPath(FileName));

    createTokens(code);

    if (!createLinks())
    {
        // Source has syntax errors, can't proceed
        return false;
    }

    // specify array size..
    arraySize();

    // simplify labels..
    labels();

    simplifyDoWhileAddBraces();
    simplifyIfAddBraces();

    // Combine "- %num%" ..
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "[([+-*/=,] - %num%") && tok->strAt(2)[0] != '-')
        {
            tok->next()->str(std::string("-") + tok->strAt(2));
            tok->next()->deleteNext();
        }

        if (Token::Match(tok, "return|case - %num%") && tok->strAt(2)[0] != '-')
        {
            tok->next()->str(std::string("-") + tok->strAt(2));
            tok->next()->deleteNext();
        }
    }

    // Combine tokens..
    for (Token *tok = _tokens; tok && tok->next(); tok = tok->next())
    {
        static const char * const combineWithNext[][3] =
        {
            { "<", "<", "<<" },

            { "&", "&", "&&" },
            { "|", "|", "||" },

            { "+", "=", "+=" },
            { "-", "=", "-=" },
            { "*", "=", "*=" },
            { "/", "=", "/=" },
            { "&", "=", "&=" },
            { "|", "=", "|=" },

            { "=", "=", "==" },
            { "!", "=", "!=" },
            { "<", "=", "<=" },
            { ">", "=", ">=" },

            { ":", ":", "::" },
            { "-", ">", "." },  // Replace "->" with "."

            { "private", ":", "private:" },
            { "protected", ":", "protected:" },
            { "public", ":", "public:" },
            { "__published", ":", "__published:" }
        };

        for (unsigned ui = 0; ui < sizeof(combineWithNext) / sizeof(combineWithNext[0]); ui++)
        {
            if (tok->str() == combineWithNext[ui][0] && tok->next()->str() == combineWithNext[ui][1])
            {
                tok->str(combineWithNext[ui][2]);
                tok->deleteNext();
            }
        }
    }

    // Remove __declspec()
    simplifyDeclspec();

    // remove calling conventions __cdecl, __stdcall..
    simplifyCallingConvention();

    // typedef..
    simplifyTypedef();

    // enum..
    simplifyEnum();

    // Remove __asm..
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok->next(), "__asm|_asm|asm {") &&
            tok->tokAt(2)->link() &&
            tok->tokAt(2)->link()->next())
        {
            Token::eraseTokens(tok, tok->tokAt(2)->link()->next());
        }

        else if (Token::Match(tok->next(), "__asm__ __volatile__ (") &&
                 tok->tokAt(3)->link() &&
                 tok->tokAt(3)->link()->next())
        {
            Token::eraseTokens(tok, tok->tokAt(3)->link()->next());
        }

        else if (Token::simpleMatch(tok->next(), "__asm"))
        {
            const Token *tok2 = tok->next();
            while (tok2 && (tok2->isNumber() || tok2->isName() || tok2->str() == ","))
                tok2 = tok2->next();
            if (tok2 && tok2->str() == ";")
                Token::eraseTokens(tok, tok2);
            else
                continue;
        }

        else
            continue;

        // insert "asm ( )"
        tok->insertToken(")");
        tok->insertToken("(");
        tok->insertToken("asm");

        Token::createMutualLinks(tok->tokAt(2), tok->tokAt(3));
    }

    // Remove "volatile", "inline" and register
    while (Token::Match(_tokens, "volatile|inline|register"))
    {
        _tokens->deleteThis();
    }
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        while (Token::Match(tok->next(), "volatile|inline|register"))
        {
            tok->deleteNext();
        }
    }

    // Remove __builtin_expect, likely and unlikely
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::simpleMatch(tok->next(), "__builtin_expect ("))
        {
            unsigned int parlevel = 0;
            for (Token *tok2 = tok->next(); tok2; tok2 = tok2->next())
            {
                if (tok2->str() == "(")
                    ++parlevel;
                else if (tok2->str() == ")")
                {
                    if (parlevel <= 1)
                        break;
                    --parlevel;
                }
                if (parlevel == 1 && tok2->str() == ",")
                {
                    if (Token::Match(tok2, ", %num% )"))
                    {
                        tok->deleteNext();
                        Token::eraseTokens(tok2->previous(), tok2->tokAt(2));
                    }
                    break;
                }
            }
        }
        else if (Token::Match(tok->next(), "likely|unlikely ("))
        {
            // remove closing ')'
            tok->tokAt(2)->link()->previous()->deleteNext();

            // remove "likely|unlikely ("
            tok->deleteNext();
            tok->deleteNext();
        }
    }

    // colapse compound standard types into a single token
    // unsigned long long int => long _isUnsigned=true,_isLong=true
    simplifyStdType();

    // Use "<" comparison instead of ">"
    simplifyComparisonOrder();

    /**
     * @todo simplify "for"
     * - move out start-statement "for (a;b;c);" => "{ a; for(;b;c); }"
     * - try to change "for" loop to a "while" loop instead
     */

    simplifyConst();

    // struct simplification "struct S {} s; => struct S { } ; S s ;
    simplifyStructDecl();

    // struct initialization (must be used before simplifyVarDecl)
    simplifyStructInit();

    // Split up variable declarations.
    simplifyVarDecl();

    simplifyVariableMultipleAssign();

    // Change initialisation of variable to assignment
    simplifyInitVar();

    // Remove redundant parantheses
    simplifyRedundantParanthesis();

    // Handle templates..
    simplifyTemplates();

    // Simplify templates.. sometimes the "simplifyTemplates" fail and
    // then unsimplified function calls etc remain. These have the
    // "wrong" syntax. So this function will just fix so that the
    // syntax is corrected.
    simplifyTemplates2();

    // Simplify the operator "?:"
    simplifyConditionOperator();

    // remove exception specifications..
    removeExceptionSpecifications(_tokens);

    // simplify function pointers
    simplifyFunctionPointers();
//updateClassList();
    setVarId();

    if (!validate())
        return false;

    return true;
}
//---------------------------------------------------------------------------

/** Specify array size if it hasn't been given */

void Tokenizer::arraySize()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "%var% [ ] = {"))
        {
            unsigned int sz = 1;
            const Token *tok2 = tok->tokAt(5);
            while (Token::Match(tok2, "%any% ,"))
            {
                sz++;
                tok2 = tok2->tokAt(2);
            }

            if (Token::Match(tok2, "%any% } ;"))
                tok->next()->insertToken(MathLib::toString<long>(sz));
        }

        else if (Token::Match(tok, "%var% [ ] = %str% ;"))
        {
            unsigned int sz = tok->strAt(4).length() - 1;
            tok->next()->insertToken(MathLib::toString<long>(sz));
        }
    }
}

/** simplify labels in the code.. add an ";" */

void Tokenizer::labels()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, ") const| {"))
        {
            // Simplify labels in the executable scope..
            unsigned int indentlevel = 0;
            while (0 != (tok = tok->next()))
            {
                // indentations..
                if (tok->str() == "{")
                    ++indentlevel;
                else if (tok->str() == "}")
                {
                    if (indentlevel <= 1)
                        break;
                    --indentlevel;
                }

                // simplify label..
                if (Token::Match(tok, "[;{}] %var% : %var%"))
                    tok->tokAt(2)->insertToken(";");
            }
        }
    }
}


/**
 * is the token pointing at a template parameters block
 * < int , 3 > => yes
 * \param tok start token that must point at "<"
 * \return true if the tokens look like template parameters
 */
static bool templateParameters(const Token *tok)
{
    if (!tok)
        return false;
    if (tok->str() != "<")
        return false;
    tok = tok->next();

    while (tok)
    {
        // num/type ..
        if (!tok->isNumber() && !tok->isName())
            return false;
        tok = tok->next();

        // optional "*"
        if (tok->str() == "*")
            tok = tok->next();

        // ,/>
        if (tok->str() == ">")
            return true;
        if (tok->str() != ",")
            break;
        tok = tok->next();
    }
    return false;
}


/**
 * Remove "template < ..." they can cause false positives because they are not expanded
 */
static void removeTemplates(Token *tok)
{
    for (; tok; tok = tok->next())
    {
        if (! Token::simpleMatch(tok, "template <"))
            continue;

        for (const Token *tok2 = tok->next(); tok2; tok2 = tok2->next())
        {
            if (tok2->str() == "{")
            {
                tok2 = tok2->link();
                tok2 = tok2 ? tok2->next() : 0;
                Token::eraseTokens(tok, tok2);
                tok->str(";");
                break;
            }
            if (tok2->str() == "(")
            {
                tok2 = tok2->link();
                if (!tok2)
                    break;
            }
            if (tok2->str() == ";")
            {
                Token::eraseTokens(tok, tok2->next());
                tok->str(";");
                break;
            }
        }
    }
}

void Tokenizer::simplifyTemplates()
{
    // Remove "typename" unless used in template arguments..
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() == "typename")
            tok->deleteThis();

        if (Token::simpleMatch(tok, "template <"))
        {
            while (tok && tok->str() != ">")
                tok = tok->next();
            if (!tok)
                break;
        }
    }

    std::set<std::string> expandedtemplates;

    // Locate specialized templates..
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() != "template")
            continue;
        if (!Token::simpleMatch(tok->next(), "< >"))
            continue;

        // what kind of template is this?
        Token *tok2 = tok->tokAt(3);
        while (tok2 && (tok2->isName() || tok2->str() == "*"))
            tok2 = tok2->next();

        if (!templateParameters(tok2))
            continue;

        // unknown template.. bail out
        if (!tok2->previous()->isName())
            continue;

        tok2 = tok2->previous();
        std::string s;
        {
            std::ostringstream ostr;
            const Token *tok3 = tok2;
            for (tok3 = tok2; tok3 && tok3->str() != ">"; tok3 = tok3->next())
            {
                if (tok3 != tok2)
                    ostr << " ";
                ostr << tok3->str();
            }
            if (!Token::Match(tok3, "> ("))
                continue;
            s = ostr.str();
        }

        // save search pattern..
        const std::string pattern(s + " > (");

        // remove spaces to create new name
        while (s.find(" ") != std::string::npos)
            s.erase(s.find(" "), 1);
        const std::string name(s + ">");
        expandedtemplates.insert(name);

        // Rename template..
        Token::eraseTokens(tok2, Token::findmatch(tok2, "("));
        tok2->str(name);

        // delete the "template < >"
        tok->deleteThis();
        tok->deleteThis();
        tok->deleteThis();

        // Use this special template in the code..
        while (0 != (tok2 = const_cast<Token *>(Token::findmatch(tok2, pattern.c_str()))))
        {
            Token::eraseTokens(tok2, Token::findmatch(tok2, "("));
            tok2->str(name);
        }
    }

    // Locate templates..
    std::list<Token *> templates;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::simpleMatch(tok, "template <"))
        {
            for (const Token *tok2 = tok; tok2; tok2 = tok2->next())
            {
                // Just a declaration => ignore this
                if (tok2->str() == ";")
                    break;

                // Implementation => add to "templates"
                if (tok2->str() == "{")
                {
                    templates.push_back(tok);
                    break;
                }
            }
        }
    }
    if (templates.empty())
    {
        removeTemplates(_tokens);
        return;
    }

    // Locate possible instantiations of templates..
    std::list<Token *> used;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        // template definition.. skip it
        if (Token::simpleMatch(tok, "template <"))
        {
            for (; tok; tok = tok->next())
            {
                if (tok->str() == "{" || tok->str() == "(")
                {
                    tok = tok->link();
                    if (!tok)
                        break;
                    if (tok->str() == "}")
                        break;
                }
                else if (tok->str() == ";")
                {
                    break;
                }
            }
            if (!tok)
                break;
        }
        else if (Token::Match(tok->previous(), "[{};=] %var% <") ||
                 Token::Match(tok->tokAt(-2), "[,:] private|protected|public %var% <"))
        {
            if (templateParameters(tok->next()))
                used.push_back(tok);
        }
    }
    if (used.empty())
    {
        removeTemplates(_tokens);
        return;
    }




    // Template arguments with default values
    for (std::list<Token *>::iterator iter1 = templates.begin(); iter1 != templates.end(); ++iter1)
    {
        std::list<Token *> eq;
        unsigned int templatepar = 1;
        std::string classname;
        for (Token *tok = *iter1; tok; tok = tok->next())
        {
            if (tok->str() == ">")
            {
                if (Token::Match(tok, "> class %var%"))
                    classname = tok->strAt(2);
                break;
            }

            if (tok->str() == ",")
                ++templatepar;

            else if (tok->str() == "=")
                eq.push_back(tok);
        }
        if (eq.empty() || classname.empty())
            continue;

        for (std::list<Token *>::iterator iter2 = used.begin(); iter2 != used.end(); ++iter2)
        {
            Token *tok = *iter2;

            if (!Token::Match(tok, (classname + " < %any%").c_str()))
                continue;

            // count the parameters..
            unsigned int usedpar = 1;
            for (tok = tok->tokAt(3); tok; tok = tok->tokAt(2))
            {
                if (tok->str() == ">")
                    break;

                if (tok->str() == ",")
                    ++usedpar;

                else
                    break;
            }
            if (tok && tok->str() == ">")
            {
                tok = tok->previous();
                std::list<Token *>::const_iterator it = eq.begin();
                for (unsigned int i = (templatepar - eq.size()); it != eq.end() && i < usedpar; ++i)
                    ++it;
                while (it != eq.end())
                {
                    tok->insertToken(",");
                    tok = tok->next();
                    tok->insertToken((*it)->strAt(1));
                    tok = tok->next();
                    ++it;
                }
            }
        }

        for (std::list<Token *>::iterator it = eq.begin(); it != eq.end(); ++it)
        {
            (*it)->deleteThis();
            (*it)->deleteThis();
        }
    }


    // expand templates
    bool done = false;
    //while (!done)
    {
        done = true;
        for (std::list<Token *>::iterator iter1 = templates.begin(); iter1 != templates.end(); ++iter1)
        {
            Token *tok = *iter1;
            std::vector<const Token *> type;
            for (tok = tok->tokAt(2); tok && tok->str() != ">"; tok = tok->next())
            {
                if (Token::Match(tok, "%var% ,|>"))
                    type.push_back(tok);
            }
            // bail out if the end of the file was reached
            if (!tok)
                break;

            // get the position of the template name
            unsigned int namepos = 0;
            if (Token::Match(tok, "> class %type% {|:"))
                namepos = 2;
            else if (Token::Match(tok, "> %type% *|&| %type% ("))
                namepos = 2;
            else if (Token::Match(tok, "> %type% %type% *|&| %type% ("))
                namepos = 3;
            else
            {
#ifndef NDEBUG
                // debug message that we bail out..
                if (_settings && _settings->_debug)
                {
                    std::cout << "simplifyTemplates debug-information: bailing out: "
                              << file(tok->tokAt(namepos))
                              << ": "
                              << tok->tokAt(namepos)->linenr()
                              << ": "
                              << tok->tokAt(namepos)->str()
                              << std::endl;
                }
#endif
                continue;
            }
            if ((tok->tokAt(namepos)->str() == "*" || tok->tokAt(namepos)->str() == "&"))
                ++namepos;

            // name of template function/class..
            const std::string name(tok->strAt(namepos));

            const bool isfunc(tok->strAt(namepos + 1) == "(");

            // locate template usage..

            std::string s(name + " <");
            for (unsigned int i = 0; i < type.size(); ++i)
            {
                if (i > 0)
                    s += ",";
                s += " %any% ";
            }
            const std::string pattern(s + "> ");

            std::string::size_type sz1 = used.size();
            unsigned int recursiveCount = 0;

            for (std::list<Token *>::iterator iter2 = used.begin(); iter2 != used.end(); ++iter2)
            {
                // If the size of "used" has changed, simplify calculations
                if (sz1 != used.size())
                {
                    sz1 = used.size();
                    simplifyCalculations();
                    recursiveCount++;
                    if (recursiveCount > 100)
                    {
                        // bail out..
                        break;
                    }
                }

                Token * const tok2 = *iter2;

                if (tok2->str() != name)
                    continue;

                if (Token::Match(tok2->previous(), "[;{}=]") &&
                    !Token::Match(tok2, (pattern + (isfunc ? "(" : "*| %var%")).c_str()))
                    continue;

                // New type..
                std::vector<Token> types2;
                s = "";
                std::string s1(name + " < ");
                for (const Token *tok3 = tok2->tokAt(2); tok3 && tok3->str() != ">"; tok3 = tok3->next())
                {
                    if (!tok3->next())
                    {
                        s.clear();
                        break;
                    }
                    s1 += tok3->str();
                    s1 += " ";
                    if (tok3->str() != ",")
                        types2.push_back(*tok3);
                    // add additional type information
                    if (tok3->isUnsigned())
                        s += "unsigned";
                    else if (tok3->isSigned())
                        s += "signed";
                    if (tok3->isLong())
                        s += "long";
                    s += tok3->str();
                }
                s1 += ">";
                const std::string type2(s);

                if (type2.empty() || type.size() != types2.size())
                {
#ifndef NDEBUG
                    std::list<ErrorLogger::ErrorMessage::FileLocation> locationList;
                    ErrorLogger::ErrorMessage::FileLocation loc;
                    loc.line = tok2->linenr();
                    loc.file = file(tok2);
                    locationList.push_back(loc);

                    const ErrorLogger::ErrorMessage errmsg(locationList,
                                                           "information",
                                                           "Failed to instantiate template. The checking continues anyway.",
                                                           "templateInstantiate");

                    _errorLogger->reportErr(errmsg);
#endif
                    if (type2.empty())
                        continue;
                    break;
                }

                // New classname/funcname..
                const std::string name2(name + "<" + type2 + ">");

                if (expandedtemplates.find(name2) == expandedtemplates.end())
                {
                    expandedtemplates.insert(name2);
                    // Copy template..
                    int _indentlevel = 0;
                    int _parlevel = 0;
                    for (const Token *tok3 = _tokens; tok3; tok3 = tok3->next())
                    {
                        if (tok3->str() == "{")
                            ++_indentlevel;
                        else if (tok3->str() == "}")
                            --_indentlevel;
                        else if (tok3->str() == "(")
                            ++_parlevel;
                        else if (tok3->str() == ")")
                            --_parlevel;

                        // Start of template..
                        if (tok3 == tok)
                        {
                            tok3 = tok3->next();
                        }

                        // member function implemented outside class definition
                        else if (_indentlevel == 0 && _parlevel == 0 && Token::Match(tok3, (pattern + " :: %var% (").c_str()))
                        {
                            addtoken(name2.c_str(), tok3->linenr(), tok3->fileIndex());
                            while (tok3->str() != "::")
                                tok3 = tok3->next();
                        }

                        // not part of template.. go on to next token
                        else
                            continue;

                        int indentlevel = 0;
                        std::stack<Token *> braces;     // holds "{" tokens
                        std::stack<Token *> brackets;   // holds "(" tokens
                        std::stack<Token *> brackets2;  // holds "[" tokens

                        for (; tok3; tok3 = tok3->next())
                        {
                            if (tok3->str() == "{")
                                ++indentlevel;

                            else if (tok3->str() == "}")
                            {
                                if (indentlevel <= 1 && brackets.empty() && brackets2.empty())
                                {
                                    // there is a bug if indentlevel is 0
                                    // the "}" token should only be added if indentlevel is 1 but I add it always intentionally
                                    // if indentlevel ever becomes 0, cppcheck will write:
                                    // ### Error: Invalid number of character {
                                    addtoken("}", tok3->linenr(), tok3->fileIndex());
                                    Token::createMutualLinks(braces.top(), _tokensBack);
                                    braces.pop();
                                    break;
                                }
                                --indentlevel;
                            }


                            if (tok3->isName())
                            {
                                // search for this token in the type vector
                                unsigned int itype = 0;
                                while (itype < type.size() && type[itype]->str() != tok3->str())
                                    ++itype;

                                // replace type with given type..
                                if (itype < type.size())
                                {
                                    addtoken(&types2[itype], tok3->linenr(), tok3->fileIndex());
                                    continue;
                                }
                            }

                            // replace name..
                            if (Token::Match(tok3, (name + " !!<").c_str()))
                            {
                                addtoken(name2.c_str(), tok3->linenr(), tok3->fileIndex());
                                continue;
                            }

                            // copy
                            addtoken(tok3, tok3->linenr(), tok3->fileIndex());
                            if (Token::Match(tok3, "%type% <"))
                            {
                                if (!Token::Match(tok3, (name + " <").c_str()))
                                    done = false;
                                used.push_back(_tokensBack);
                            }

                            // link() newly tokens manually
                            if (tok3->str() == "{")
                            {
                                braces.push(_tokensBack);
                            }
                            else if (tok3->str() == "}")
                            {
                                assert(braces.empty() == false);
                                Token::createMutualLinks(braces.top(), _tokensBack);
                                braces.pop();
                            }
                            else if (tok3->str() == "(")
                            {
                                brackets.push(_tokensBack);
                            }
                            else if (tok3->str() == "[")
                            {
                                brackets2.push(_tokensBack);
                            }
                            else if (tok3->str() == ")")
                            {
                                assert(brackets.empty() == false);
                                Token::createMutualLinks(brackets.top(), _tokensBack);
                                brackets.pop();
                            }
                            else if (tok3->str() == "]")
                            {
                                assert(brackets2.empty() == false);
                                Token::createMutualLinks(brackets2.top(), _tokensBack);
                                brackets2.pop();
                            }

                        }

                        assert(braces.empty());
                        assert(brackets.empty());
                    }
                }

                // Replace all these template usages..
                for (Token *tok4 = tok2; tok4; tok4 = tok4->next())
                {
                    if (Token::simpleMatch(tok4, s1.c_str()))
                    {
                        bool match = true;
                        Token * tok5 = tok4->tokAt(2);
                        unsigned int count = 0;
                        while (tok5->str() != ">")
                        {
                            if (tok5->str() != ",")
                            {
                                if (tok5->isUnsigned() != types2[count].isUnsigned() ||
                                    tok5->isSigned() != types2[count].isSigned() ||
                                    tok5->isLong() != types2[count].isLong())
                                {
                                    match = false;
                                    break;
                                }
                                count++;
                            }
                            tok5 = tok5->next();
                        }

                        if (match)
                        {
                            tok4->str(name2);
                            while (tok4->next()->str() != ">")
                            {
                                used.remove(tok4->next());
                                tok4->deleteNext();
                            }
                            used.remove(tok4->next());
                            tok4->deleteNext();
                        }
                    }
                }
            }
        }
    }

    removeTemplates(_tokens);
}
//---------------------------------------------------------------------------

void Tokenizer::simplifyTemplates2()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() == "(")
            tok = tok->link();

        else if (Token::Match(tok, "; %type% <"))
        {
            const Token *tok2 = tok->tokAt(3);
            std::string type;
            while (Token::Match(tok2, "%type% ,") || Token::Match(tok2, "%num% ,"))
            {
                type += tok2->str() + ",";
                tok2 = tok2->tokAt(2);
            }
            if (Token::Match(tok2, "%type% > (") || Token::Match(tok2, "%num% > ("))
            {
                type += tok2->str();
                tok = tok->next();
                tok->str(tok->str() + "<" + type + ">");
                Token::eraseTokens(tok, tok2->tokAt(2));
            }
        }
    }
}
//---------------------------------------------------------------------------

std::string Tokenizer::getNameForFunctionParams(const Token *start)
{
    if (start->next() == start->link())
        return "";

    std::string result;
    bool findNextComma = false;
    for (const Token *tok = start->next(); tok && tok != start->link(); tok = tok->next())
    {
        if (findNextComma)
        {
            if (tok->str() == ",")
                findNextComma = false;

            continue;
        }

        result.append(tok->str() + ",");
        findNextComma = true;
    }

    return result;
}

void Tokenizer::updateClassList()
{
    _classInfoList.clear();

    // Locate class
    const Token *tok1 = tokens();

    // Locate implementation
    tok1 = tokens();
    std::map<std::string, std::map<std::string, std::map<std::string, const Token*> > > implementations;
    while ((tok1 = Token::findmatch(tok1, "%var% :: ~| %var% (")) != 0)
    {
        if (tok1->tokAt(2)->str() == "~")
            implementations[tok1->str()]["~ "+tok1->tokAt(3)->str()][getNameForFunctionParams(tok1->tokAt(4))] = tok1;
        else
            implementations[tok1->str()][tok1->tokAt(2)->str()][getNameForFunctionParams(tok1->tokAt(3))] = tok1;
//std::cout <<"1."<< tok1->str() << "," << tok1->tokAt(2)->str() << ","<<getNameForFunctionParams(tok1->tokAt(3));
        tok1 = tok1->next();
    }

    tok1 = tokens();
    while ((tok1 = Token::findmatch(tok1, "%var% :: operator %any% (")) != 0)
    {
        implementations[tok1->str()][tok1->tokAt(2)->str()+" "+tok1->tokAt(3)->str()][getNameForFunctionParams(tok1->tokAt(4))] = tok1;
//std::cout <<"3."<< tok1->str() << "," << tok1->tokAt(2)->str()+" "+tok1->tokAt(3)->str() << ","<<getNameForFunctionParams(tok1->tokAt(4));
        tok1 = tok1->next();
    }

    tok1 = tokens();
    while ((tok1 = Token::findmatch(tok1, "class|struct %var% [{:]")) != 0)
    {
        ClassInfo::MemberType memberType = ClassInfo::PRIVATE;
        if (tok1->str() == "struct")
            memberType = ClassInfo::PUBLIC;

        const std::string className = tok1->strAt(1);
        tok1 = tok1->next();
        int indentlevel = 0;
        for (const Token *tok = tok1; tok; tok = tok->next())
        {
            // Indentation
            if (tok->str() == "{")
            {
                ++indentlevel;
                continue;
            }

            else if (tok->str() == "}")
            {
                --indentlevel;
                if (indentlevel <= 0)
                    break;

                continue;
            }

            // Parse class contents (indentlevel == 1)..
            if (indentlevel == 1)
            {
                if (tok->str() == "private:")
                    memberType = ClassInfo::PRIVATE;
                else if (tok->str() == "protected:")
                    memberType = ClassInfo::PROTECTED;
                else if (tok->str() == "public:")
                    memberType = ClassInfo::PUBLIC;

                else if (Token::Match(tok, "typedef %type% ("))
                    tok = tok->tokAt(2);

                else if (Token::Match(tok, "[:,] %var% ("))
                    tok = tok->tokAt(2);

                else if (Token::Match(tok, "%var% ("))
                {
                    ClassInfo::MemberFunctionInfo func;
                    if (tok->previous()->str() == "~")
                    {
                        // destructor
                        func._declaration = tok->previous();
                        func._name = "~ " + tok->str();
                    }
                    else
                    {
                        // member function
                        func._declaration = tok;
                        func._name = tok->str();
                    }

                    func._type = memberType;
                    if (tok->next()->link()->next()->str() == "{")
                        func._implementation = tok;
                    else
                        func._implementation = implementations[className][func._name][getNameForFunctionParams(tok->next())];
//std::cout <<"\n2."<< className << "," << func._name << ","<<getNameForFunctionParams(tok->next())<<"\n";
                    _classInfoList[className]._memberFunctions.push_back(func);
                }
                else if (Token::Match(tok, "operator %any% ("))
                {
                    ClassInfo::MemberFunctionInfo func;
                    func._declaration = tok;
                    func._name = tok->str() + " " + tok->next()->str();
                    func._type = memberType;
                    if (tok->tokAt(2)->link()->next()->str() == "{")
                        func._implementation = tok;
                    else
                        func._implementation = implementations[className][func._name][getNameForFunctionParams(tok->tokAt(2))];

//std::cout <<"\n4."<< className << "," << func._name << ","<<getNameForFunctionParams(tok->tokAt(2))<<"\n";
                    _classInfoList[className]._memberFunctions.push_back(func);
                }
            }
        }
    }
}


void Tokenizer::setVarId()
{
    // Clear all variable ids
    for (Token *tok = _tokens; tok; tok = tok->next())
        tok->varId(0);

    // Set variable ids..
    unsigned int _varId = 0;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok != _tokens && !Token::Match(tok, "[,;{}(] %type%"))
            continue;

        // If pattern is "( %type% *|& %var% )" then check if it's a
        // variable declaration or a multiplication / mask
        if (Token::Match(tok, "( %type% *|& %var% )") && !tok->next()->isStandardType())
        {
            if (!Token::Match(tok->previous(), "%type%"))
                continue;
            if (tok->strAt(-1) == "return")
                continue;
            if (!Token::Match(tok->tokAt(5), "const|{|;"))
                continue;
        }

        if (Token::Match(tok, "[,;{}(] %type%"))
            tok = tok->next();

        if (tok->str() == "new")
            continue;

        if (tok->str() == "unsigned")
            tok = tok->next();

        if (Token::Match(tok, "class|struct %type% :|{|;"))
            continue;

        if (Token::Match(tok, "using namespace %type% ;"))
        {
            tok = tok->next();
            continue;
        }

        if (Token::Match(tok, "else|return|typedef|delete|sizeof"))
            continue;

        if (Token::Match(tok, "const|static|extern|public:|private:|protected:"))
            tok = tok->next();

        while (Token::Match(tok, "%var% ::"))
            tok = tok->tokAt(2);

        // Skip template arguments..
        if (Token::Match(tok, "%type% <"))
        {
            int level = 1;
            bool again;
            Token *tok2 = tok->tokAt(2);

            do // Look for start of templates or template arguments
            {
                again = false;

                while (Token::Match(tok2, "%var% ::"))
                    tok2 = tok2->tokAt(2);

                if (Token::Match(tok2, "%type% <"))
                {
                    level++;
                    tok2 = tok2->tokAt(2);
                    again = true;
                }
                else if (Token::Match(tok2, "%type% ,"))
                {
                    tok2 = tok2->tokAt(2);
                    again = true;
                }
                else
                {
                    while (tok2 && (tok2->isName() || tok2->isNumber() || tok2->str() == "*" || tok2->str() == ","))
                        tok2 = tok2->next();
                }
            }
            while (again);

            do // Look for end of templates
            {
                again = false;

                if (level == 1 && Token::Match(tok2, "> %var%"))
                    tok = tok2;
                else if (level > 1 && tok2->str() == ">")
                {
                    level--;
                    if (level == 0)
                        tok = tok2;
                    else
                    {
                        tok2 = tok2->tokAt(1);
                        again = true;
                    }
                }
                else if (level == 1 && Token::Match(tok2, "> ::|*|& %var%"))
                    tok = tok2->next();
                else
                    continue;       // Not code that I understand / not a variable declaration
            }
            while (again);
        }

        // Determine name of declared variable..
        std::string varname;
        Token *tok2 = tok->tokAt(1);
        while (tok2)
        {
            if (tok2->isName())
                varname = tok2->str();
            else if (tok2->str() != "*" && tok2->str() != "&")
                break;
            tok2 = tok2->next();
        }

        // End of tokens reached..
        if (!tok2)
            break;

        if (varname == "operator" && Token::Match(tok2, "=|+|-|*|/|[| ]| ("))
            continue;

        // Is it a function?
        if (tok2->str() == "(")
        {
            // Search for function declaration, e.g. void f();
            if (Token::simpleMatch(tok2->next(), ") ;"))
                continue;

            // Search for function declaration, e.g. void f( int c );
            if (Token::Match(tok2->next(), "%num%") ||
                Token::Match(tok2->next(), "%bool%") ||
                tok2->next()->str()[0] == '"' ||
                tok2->next()->varId() != 0)
            {
                // This is not a function
            }
            else
            {
                continue;
            }
        }

        // Variable declaration found => Set variable ids
        if (Token::Match(tok2, "[,();[=]") && !varname.empty())
        {
            ++_varId;
            int indentlevel = 0;
            int parlevel = 0;
            bool dot = false;
            bool funcDeclaration = false;
            for (tok2 = tok->next(); tok2; tok2 = tok2->next())
            {
                if (!dot && tok2->str() == varname && !Token::Match(tok2->previous(), "struct|union"))
                    tok2->varId(_varId);
                else if (tok2->str() == "{")
                    ++indentlevel;
                else if (tok2->str() == "}")
                {
                    --indentlevel;
                    if (indentlevel < 0)
                        break;

                    // We have reached the end of a loop: "for( int i;;) {  }"
                    if (funcDeclaration && indentlevel <= 0)
                        break;
                }
                else if (tok2->str() == "(")
                    ++parlevel;
                else if (tok2->str() == ")")
                {
                    // Is this a function parameter or a variable declared in for example a for loop?
                    if (parlevel == 0 && indentlevel == 0 && Token::Match(tok2, ") const| {"))
                        funcDeclaration = true;
                    else
                        --parlevel;
                }
                else if (parlevel < 0 && tok2->str() == ";")
                    break;
                dot = bool(tok2->str() == ".");
            }
        }
    }

    // Struct/Class members
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        // str.clear is a variable
        // str.clear() is a member function
        if (tok->varId() != 0 &&
            Token::Match(tok->next(), ". %var% !!(") &&
            tok->tokAt(2)->varId() == 0)
        {
            ++_varId;

            const std::string pattern(std::string("%varid% . ") + tok->strAt(2));
            for (Token *tok2 = tok; tok2; tok2 = tok2->next())
            {
                if (Token::Match(tok2, pattern.c_str(), tok->varId()))
                    tok2->tokAt(2)->varId(_varId);
            }
        }
    }

    // Member functions and variables in this source
    std::list<Token *> allMemberFunctions;
    std::list<Token *> allMemberVars;
    {
        for (Token *tok2 = _tokens; tok2; tok2 = tok2->next())
        {
            if (Token::Match(tok2, "%var% :: %var%"))
            {
                if (Token::simpleMatch(tok2->tokAt(3), "("))
                    allMemberFunctions.push_back(tok2);
                else if (tok2->tokAt(2)->varId() != 0)
                    allMemberVars.push_back(tok2);
            }
        }
    }

    // class members..
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "class %var% {"))
        {
            const std::string &classname(tok->next()->str());


            // What member variables are there in this class?
            std::map<std::string, unsigned int> varlist;
            {
                unsigned int indentlevel = 0;
                for (const Token *tok2 = tok; tok2; tok2 = tok2->next())
                {
                    // Indentation..
                    if (tok2->str() == "{")
                        ++indentlevel;
                    else if (tok2->str() == "}")
                    {
                        if (indentlevel <= 1)
                            break;
                        --indentlevel;
                    }

                    // Found a member variable..
                    else if (indentlevel == 1 && tok2->varId() > 0)
                        varlist[tok2->str()] = tok2->varId();
                }
            }

            // Are there any member variables in this class?
            if (varlist.empty())
                continue;

            // Member variables
            for (std::list<Token *>::iterator func = allMemberVars.begin(); func != allMemberVars.end(); ++func)
            {
                if (!Token::simpleMatch(*func, classname.c_str()))
                    continue;

                Token *tok2 = *func;
                tok2 = tok2->tokAt(2);
                tok2->varId(varlist[tok2->str()]);
            }

            // Member functions for this class..
            std::list<Token *> funclist;
            {
                const std::string funcpattern(classname + " :: %var% (");
                for (std::list<Token *>::iterator func = allMemberFunctions.begin(); func != allMemberFunctions.end(); ++func)
                {
                    Token *tok2 = *func;

                    // Found a class function..
                    if (Token::Match(tok2, funcpattern.c_str()))
                    {
                        // Goto the end paranthesis..
                        tok2 = tok2->tokAt(3)->link();
                        if (!tok2)
                            break;

                        // If this is a function implementation.. add it to funclist
                        if (Token::Match(tok2, ") const|volatile| {"))
                            funclist.push_back(tok2);
                    }
                }
            }

            // Update the variable ids..
            // Parse each function..
            for (std::list<Token *>::iterator func = funclist.begin(); func != funclist.end(); ++func)
            {
                unsigned int indentlevel = 0;
                for (Token *tok2 = *func; tok2; tok2 = tok2->next())
                {
                    if (tok2->str() == "{")
                        ++indentlevel;
                    else if (tok2->str() == "}")
                    {
                        if (indentlevel <= 1)
                            break;
                        --indentlevel;
                    }
                    else if (indentlevel > 0 &&
                             tok2->varId() == 0 &&
                             varlist.find(tok2->str()) != varlist.end())
                    {
                        tok2->varId(varlist[tok2->str()]);
                    }
                }
            }

        }
    }
}


//---------------------------------------------------------------------------
// Simplify token list
//---------------------------------------------------------------------------

void Tokenizer::simplifyNamespaces()
{
    for (Token *token = _tokens; token; token = token->next())
    {
        while (token && token->str() == "namespace" && token->varId() == 0 &&
               (!token->previous() || token->previous()->str() != "using"))
        {
            // Token is namespace and there is no "using" before it.
            Token *start = token;
            Token *tok = token->tokAt(2);
            if (!tok)
                return;

            tok = tok->link();
            if (tok && tok->str() == "}")
            {
                tok = tok->previous();
                tok->deleteNext();
                start->deleteNext();
                start->deleteNext();
                if (start->previous())
                {
                    token = start->next();
                    start = start->previous();
                    start->deleteNext();
                }
                else
                {
                    // First token in the list, don't delete
                    // as _tokens is attached to it.
                    start->deleteThis();
                }
            }
            else
            {
                return;
            }
        }

        if (!token)
            break;
    }
}

bool Tokenizer::createLinks()
{
    std::list<const Token*> type;
    std::list<Token*> links;
    std::list<Token*> links2;
    std::list<Token*> links3;
    for (Token *token = _tokens; token; token = token->next())
    {
        if (token->link())
        {
            token->link(0);
        }

        if (token->str() == "{")
        {
            links.push_back(token);
            type.push_back(token);
        }
        else if (token->str() == "}")
        {
            if (links.empty())
            {
                // Error, { and } don't match.
                syntaxError(token, '{');
                return false;
            }
            if (type.back()->str() != "{")
            {
                syntaxError(type.back(), type.back()->str()[0]);
                return false;
            }
            type.pop_back();

            Token::createMutualLinks(links.back(), token);
            links.pop_back();
        }
        else if (token->str() == "(")
        {
            links2.push_back(token);
            type.push_back(token);
        }
        else if (token->str() == ")")
        {
            if (links2.empty())
            {
                // Error, ( and ) don't match.
                syntaxError(token, '(');
                return false;
            }
            if (type.back()->str() != "(")
            {
                syntaxError(type.back(), type.back()->str()[0]);
                return false;
            }
            type.pop_back();

            Token::createMutualLinks(links2.back(), token);
            links2.pop_back();
        }
        else if (token->str() == "[")
        {
            links3.push_back(token);
            type.push_back(token);
        }
        else if (token->str() == "]")
        {
            if (links3.empty())
            {
                // Error, [ and ] don't match.
                syntaxError(token, '[');
                return false;
            }
            if (type.back()->str() != "[")
            {
                syntaxError(type.back(), type.back()->str()[0]);
                return false;
            }
            type.pop_back();

            Token::createMutualLinks(links3.back(), token);
            links3.pop_back();
        }
    }

    if (!links.empty())
    {
        // Error, { and } don't match.
        syntaxError(links.back(), '{');
        return false;
    }

    if (!links2.empty())
    {
        // Error, ( and ) don't match.
        syntaxError(links2.back(), '(');
        return false;
    }

    if (!links3.empty())
    {
        // Error, [ and ] don't match.
        syntaxError(links3.back(), '[');
        return false;
    }

    return true;
}

void Tokenizer::simplifySizeof()
{
    // Fill the map _typeSize..
    _typeSize.clear();
    _typeSize["char"] = sizeof(char);
    _typeSize["short"] = sizeof(short);
    _typeSize["int"] = sizeof(int);
    _typeSize["long"] = sizeof(long);
    _typeSize["float"] = sizeof(float);
    _typeSize["double"] = sizeof(double);
    _typeSize["size_t"] = sizeof(size_t);
    _typeSize["*"] = sizeof(void *);

    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "class|struct %var%"))
        {
            _typeSize[tok->strAt(1)] = 100;
        }
    }

    // Locate variable declarations and calculate the size
    std::map<unsigned int, std::string> sizeOfVar;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->varId() != 0 && sizeOfVar.find(tok->varId()) == sizeOfVar.end())
        {
            const unsigned int varId = tok->varId();
            if (Token::Match(tok->tokAt(-3), "[;{}(,] %type% * %var% [;,)]") ||
                Token::Match(tok->tokAt(-4), "[;{}(,] const %type% * %var% [;),]") ||
                Token::Match(tok->tokAt(-2), "[;{}(,] %type% %var% [;),]") ||
                Token::Match(tok->tokAt(-3), "[;{}(,] const %type% %var% [;),]"))
            {
                const unsigned int size = sizeOfType(tok->previous());
                if (size == 0)
                {
                    continue;
                }

                sizeOfVar[varId] = MathLib::toString<long>(size);
            }

            else if (Token::Match(tok->tokAt(-1), "%type% %var% [ %num% ] [;=]") ||
                     Token::Match(tok->tokAt(-2), "%type% * %var% [ %num% ] [;=]"))
            {
                unsigned int size = sizeOfType(tok->tokAt(-1));
                if (size == 0)
                    continue;

                sizeOfVar[varId] = MathLib::toString<long>(size * MathLib::toLongNumber(tok->strAt(2)));
            }

            else if (Token::Match(tok->tokAt(-1), "%type% %var% [ %num% ] [,)]") ||
                     Token::Match(tok->tokAt(-2), "%type% * %var% [ %num% ] [,)]"))
            {
                Token tempTok(0);
                tempTok.str("*");
                sizeOfVar[varId] = MathLib::toString<long>(sizeOfType(&tempTok));
            }

            else if (Token::Match(tok->tokAt(-1), "%type% %var% [ ] = %str% ;"))
            {
                unsigned int size = sizeOfType(tok->tokAt(4));
                if (size == 0)
                    continue;

                sizeOfVar[varId] = MathLib::toString<long>(size);
            }
        }
    }

    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() != "sizeof")
            continue;

        // sizeof "text"
        if (Token::Match(tok->next(), "%str%"))
        {
            tok->deleteThis();
            std::ostringstream ostr;
            ostr << (Token::getStrLength(tok) + 1);
            tok->str(ostr.str());
            continue;
        }

        // sizeof ("text")
        if (Token::Match(tok->next(), "( %str% )"))
        {
            tok->deleteThis();
            tok->deleteThis();
            tok->deleteNext();
            std::ostringstream ostr;
            ostr << (Token::getStrLength(tok) + 1);
            tok->str(ostr.str());
            continue;
        }

        // sizeof * (...) -> sizeof(*...)
        if (Token::simpleMatch(tok->next(), "* (") && !Token::simpleMatch(tok->tokAt(2)->link(), ") ."))
        {
            tok->deleteNext();
            tok->next()->insertToken("*");
        }

        // sizeof a++ -> sizeof(a++)
        if (Token::Match(tok->next(), "++|-- %var% !!.") || Token::Match(tok->next(), "%var% ++|--"))
        {
            tok->insertToken("(");
            tok->tokAt(3)->insertToken(")");
            Token::createMutualLinks(tok->next(), tok->tokAt(4));
        }

        // sizeof 1 => sizeof ( 1 )
        if (tok->next()->isNumber())
        {
            Token *tok2 = tok->next();
            tok->insertToken("(");
            tok2->insertToken(")");
            Token::createMutualLinks(tok->next(), tok2->next());
        }

        // sizeof int -> sizeof( int )
        else if (tok->next()->str() != "(")
        {
            // Add parenthesis around the sizeof
            for (Token *tempToken = tok->next(); tempToken; tempToken = tempToken->next())
            {
                if (Token::Match(tempToken, "%var%"))
                {
                    while (tempToken->next()->str() == "[")
                    {
                        tempToken = tempToken->next()->link();
                    }

                    if (tempToken->next()->str() == ".")
                    {
                        // We are checking a class or struct, search next varname
                        tempToken = tempToken->tokAt(1);
                        continue;
                    }
                    else if (Token::simpleMatch(tempToken->next(), "- >"))
                    {
                        // We are checking a class or struct, search next varname
                        tempToken = tempToken->tokAt(2);
                        continue;
                    }
                    else if (Token::Match(tempToken->next(), "++|--"))
                    {
                        // We have variable++ or variable--, there should be
                        // nothing after this
                        tempToken = tempToken->tokAt(2);
                    }
                    else if (Token::simpleMatch(tempToken->next(), ") ."))
                    {
                        tempToken = tempToken->tokAt(2);
                        continue;
                    }

                    // Ok, we should be clean. Add ) after tempToken
                    tok->insertToken("(");
                    tempToken->insertToken(")");
                    Token::createMutualLinks(tok->next(), tempToken->next());
                    break;
                }
            }
        }

        // sizeof(type *) => sizeof(*)
        if (Token::Match(tok->next(), "( %type% * )"))
        {
            tok->next()->deleteNext();
        }

        if (Token::Match(tok->next(), "( * )"))
        {
            tok->str(MathLib::toString<long>(sizeOfType(tok->tokAt(2))));
            Token::eraseTokens(tok, tok->tokAt(4));
        }

        // sizeof( a )
        else if (Token::Match(tok->next(), "( %var% )") && tok->tokAt(2)->varId() != 0)
        {
            if (sizeOfVar.find(tok->tokAt(2)->varId()) != sizeOfVar.end())
            {
                tok->deleteThis();
                tok->deleteThis();
                tok->deleteNext();
                tok->str(sizeOfVar[tok->varId()]);
            }
            else
            {
                // don't try to replace size of variable if variable has
                // similar name with type (#329)
            }
        }

        else if (Token::Match(tok, "sizeof ( %type% )"))
        {
            unsigned int size = sizeOfType(tok->tokAt(2));
            if (size > 0)
            {
                tok->str(MathLib::toString<long>(size));
                Token::eraseTokens(tok, tok->tokAt(4));
            }
        }

        else if (Token::Match(tok, "sizeof ( * %var% )") || Token::Match(tok, "sizeof ( %var% [ %num% ] )"))
        {
            // Some default value..
            unsigned int sz = 0;

            unsigned int varid = tok->tokAt((tok->tokAt(2)->str() == "*") ? 3 : 2)->varId();
            if (varid != 0)
            {
                // Try to locate variable declaration..
                const Token *decltok = Token::findmatch(_tokens, "%varid%", varid);
                if (Token::Match(decltok->previous(), "%type% %var% ["))
                {
                    sz = sizeOfType(decltok->previous());
                }
                else if (Token::Match(decltok->previous(), "* %var% ["))
                {
                    sz = sizeOfType(decltok->previous());
                }
                else if (Token::Match(decltok->tokAt(-2), "%type% * %var%"))
                {
                    sz = sizeOfType(decltok->tokAt(-2));
                }
            }

            if (sz > 0)
            {
                tok->str(MathLib::toString<long>(sz));
                Token::eraseTokens(tok, tok->next()->link()->next());
            }
        }
    }

}

bool Tokenizer::simplifyTokenList()
{
    // clear the _functionList so it can't contain dead pointers
    _functionList.clear();

    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::simpleMatch(tok, "* const"))
            tok->deleteNext();
    }


    // Replace NULL with 0..
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() == "NULL" || tok->str() == "'\\0'" || tok->str() == "'\\x0'")
        {
            tok->str("0");
        }
        else if (tok->isNumber() &&
                 MathLib::isInt(tok->str()) &&
                 MathLib::toLongNumber(tok->str()) == 0)
        {
            tok->str("0");
        }
    }

    // simplify references
    simplifyReference();

    simplifyStd();

    simplifyNamespaces();

    simplifyGoto();

    // Combine wide strings
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        while (tok->str() == "L" && tok->next() && tok->next()->str()[0] == '"')
        {
            // Combine 'L "string"'
            tok->str(tok->next()->str());
            tok->deleteNext();
        }
    }

    // Combine strings
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str()[0] != '"')
            continue;

        tok->str(simplifyString(tok->str()));
        while (tok->next() && tok->next()->str()[0] == '"')
        {
            tok->next()->str(simplifyString(tok->next()->str()));

            // Two strings after each other, combine them
            tok->concatStr(tok->next()->str());
            tok->deleteNext();
        }
    }

    // Convert e.g. atol("0") into 0
    simplifyMathFunctions();

    // Convert + + into + and + - into -
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        while (tok->next())
        {
            if (tok->str() == "+")
            {
                if (tok->next()->str() == "+")
                {
                    tok->deleteNext();
                    continue;
                }
                else if (tok->next()->str() == "-")
                {
                    tok->str("-");
                    tok->deleteNext();
                    continue;
                }
            }
            else if (tok->str() == "-")
            {
                if (tok->next()->str() == "-")
                {
                    tok->str("+");
                    tok->deleteNext();
                    continue;
                }
                else if (tok->next()->str() == "+")
                {
                    tok->deleteNext();
                    continue;
                }
            }

            break;
        }
    }

    // 0[a] -> a[0]
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "%num% [ %var% ]"))
        {
            const std::string temp = tok->str();
            tok->str(tok->tokAt(2)->str());
            tok->tokAt(2)->str(temp);
        }
    }

    simplifySizeof();

    // replace strlen(str)
    simplifyKnownVariables();
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "strlen ( %str% )"))
        {
            std::ostringstream ostr;
            ostr << Token::getStrLength(tok->tokAt(2));
            tok->str(ostr.str());
            tok->deleteNext();
            tok->deleteNext();
            tok->deleteNext();
        }
    }

    // change array to pointer..
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "%type% %var% [ ] [,;=]"))
        {
            Token::eraseTokens(tok->next(), tok->tokAt(4));
            tok->insertToken("*");
        }
    }

    // Replace constants..
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "const %type% %var% = %num% ;"))
        {
            unsigned int varId = tok->tokAt(2)->varId();
            if (varId == 0)
            {
                tok = tok->tokAt(5);
                continue;
            }

            const std::string num = tok->strAt(4);
            int indent = 1;
            for (Token *tok2 = tok->tokAt(6); tok2; tok2 = tok2->next())
            {
                if (tok2->str() == "{")
                {
                    ++indent;
                }
                else if (tok2->str() == "}")
                {
                    --indent;
                    if (indent == 0)
                        break;
                }

                // Compare constants, but don't touch members of other structures
                else if (tok2->varId() == varId)
                {
                    tok2->str(num);
                }
            }
        }
    }

    simplifyLogicalOperators();
    simplifyCasts();

    // simplify "x=realloc(y,0);" => "free(y); x=0;"..
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "; %var% = realloc ( %var% , 0 ) ;"))
        {
            const std::string varname(tok->next()->str());
            const unsigned int varid(tok->next()->varId());

            // Delete the "%var% ="
            tok->deleteNext();
            tok->deleteNext();

            // Change function name "realloc" to "free"
            tok->next()->str("free");

            // delete the ", 0"
            Token::eraseTokens(tok->tokAt(3), tok->tokAt(6));

            // goto the ";"
            tok = tok->tokAt(5);

            // insert "var=0;"
            tok->insertToken(";");
            tok->insertToken("0");
            tok->insertToken("=");
            tok->insertToken(varname);
            tok->next()->varId(varid);
        }
    }

    // Simplify simple calculations..
    while (simplifyCalculations())
        ;

    // Replace "*(str + num)" => "str[num]"
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (! strchr(";{}(=<>", tok->str()[0]))
            continue;

        Token *next = tok->next();
        if (! next)
            break;

        if (Token::Match(next, "* ( %var% + %num% )"))
        {
            // var
            tok = tok->next();
            tok->str(tok->strAt(2));

            // [
            tok = tok->next();
            tok->str("[");

            // num
            tok = tok->next();
            tok->str(tok->strAt(2));

            // ]
            tok = tok->next();
            tok->str("]");

            tok->deleteNext();
            tok->deleteNext();

            Token::createMutualLinks(next->tokAt(1), next->tokAt(3));
        }
    }

    // Change initialisation of variable to assignment
    simplifyInitVar();

    // Simplify variable declarations
    simplifyVarDecl();

    simplifyFunctionParameters();
    elseif();
    simplifyIfAssign();
    simplifyRedundantParanthesis();
    simplifyIfNot();
    simplifyIfNotNull();
    simplifyIfSameInnerCondition();
    simplifyComparisonOrder();
    simplifyNestedStrcat();
    simplifyWhile0();
    simplifyFuncInWhile();

    simplifyIfAssign();    // could be affected by simplifyIfNot

    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "case %any% : %var%"))
            tok->tokAt(2)->insertToken(";");
        if (Token::Match(tok, "default : %var%"))
            tok->next()->insertToken(";");
    }

    // In case variable declarations have been updated...
    setVarId();

    bool modified = true;
    while (modified)
    {
        modified = false;
        modified |= simplifyConditions();
        modified |= simplifyFunctionReturn();
        modified |= simplifyKnownVariables();
        modified |= removeReduntantConditions();
        modified |= simplifyRedundantParanthesis();
        modified |= simplifyQuestionMark();
        modified |= simplifyCalculations();
    }

    // Remove redundant parantheses in return..
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        while (Token::simpleMatch(tok, "return ("))
        {
            Token *tok2 = tok->next()->link();
            if (Token::simpleMatch(tok2, ") ;"))
            {
                tok->deleteNext();
                tok2->deleteThis();
            }
            else
            {
                break;
            }
        }
    }

    removeRedundantAssignment();

    simplifyComma();
    if (_settings && _settings->_debug)
    {
        _tokens->printOut(0, _files);
    }

    return validate();
}
//---------------------------------------------------------------------------

void Tokenizer::removeRedundantAssignment()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() == "{")
            tok = tok->link();

        if (Token::Match(tok, ") const| {"))
        {
            // parse in this function..
            std::set<unsigned int> localvars;
            if (tok->next()->str() == "const")
                tok = tok->next();
            const Token * const end = tok->next()->link();
            for (Token *tok2 = tok->next(); tok2 && tok2 != end; tok2 = tok2->next())
            {
                if (Token::Match(tok2, "[;{}] %type% * %var% ;"))
                {
                    tok2 = tok2->tokAt(3);
                    localvars.insert(tok2->varId());
                }
                else if (Token::Match(tok2, "[;{}] %type% %var% ;") && tok2->next()->isStandardType())
                {
                    tok2 = tok2->tokAt(2);
                    localvars.insert(tok2->varId());
                }
                else if (tok2->varId() &&
                         !Token::Match(tok2->previous(), "[;{}] %var% = %var% ;") &&
                         !Token::Match(tok2->previous(), "[;{}] %var% = %num% ;"))
                {
                    localvars.erase(tok2->varId());
                }
            }
            localvars.erase(0);
            if (!localvars.empty())
            {
                for (Token *tok2 = tok->next(); tok2 && tok2 != end; tok2 = tok2->next())
                {
                    if (Token::Match(tok2, "[;{}] %type% %var% ;") && localvars.find(tok2->tokAt(2)->varId()) != localvars.end())
                    {
                        Token::eraseTokens(tok2, tok2->tokAt(3));
                    }
                    else if (Token::Match(tok2, "[;{}] %type% * %var% ;") && localvars.find(tok2->tokAt(3)->varId()) != localvars.end())
                    {
                        Token::eraseTokens(tok2, tok2->tokAt(4));
                    }
                    else if (Token::Match(tok2, "[;{}] %var% = %any% ;") && localvars.find(tok2->next()->varId()) != localvars.end())
                    {
                        Token::eraseTokens(tok2, tok2->tokAt(4));
                    }
                }
            }
        }
    }
}

bool Tokenizer::removeReduntantConditions()
{
    bool ret = false;

    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() != "if")
            continue;

        if (!Token::Match(tok->tokAt(1), "( %bool% ) {"))
            continue;

        // Find matching else
        const Token *elseTag = 0;

        // Find the closing "}"
        elseTag = tok->tokAt(4)->link()->next();

        bool boolValue = false;
        if (tok->tokAt(2)->str() == "true")
            boolValue = true;

        // Handle if with else
        if (elseTag && elseTag->str() == "else")
        {
            if (Token::simpleMatch(elseTag->next(), "if ("))
            {
                // Handle "else if"
                if (boolValue == false)
                {
                    // Convert "if( false ) {aaa;} else if() {bbb;}" => "if() {bbb;}"
                    Token::eraseTokens(tok, elseTag->tokAt(2));
                    ret = true;
                }
                else
                {
                    // Keep first if, remove every else if and else after it
                    const Token *lastTagInIf = elseTag->tokAt(2);
                    while (lastTagInIf)
                    {
                        if (lastTagInIf->str() == "(")
                        {
                            lastTagInIf = lastTagInIf->link()->next();
                        }

                        lastTagInIf = lastTagInIf->link()->next();
                        if (!Token::simpleMatch(lastTagInIf, "else"))
                            break;

                        lastTagInIf = lastTagInIf->next();
                        if (lastTagInIf->str() == "if")
                            lastTagInIf = lastTagInIf->next();
                    }

                    Token::eraseTokens(elseTag->previous(), lastTagInIf);
                    ret = true;
                }
            }
            else
            {
                // Handle else
                if (boolValue == false)
                {
                    // Convert "if( false ) {aaa;} else {bbb;}" => "{bbb;}" or ";{bbb;}"
                    if (tok->previous())
                        tok = tok->previous();
                    else
                        tok->str(";");

                    Token::eraseTokens(tok, elseTag->tokAt(1));
                }
                else
                {
                    if (elseTag->tokAt(1)->str() == "{")
                    {
                        // Convert "if( true ) {aaa;} else {bbb;}" => "{aaa;}"
                        const Token *end = elseTag->tokAt(1)->link();

                        // Remove the "else { aaa; }"
                        Token::eraseTokens(elseTag->previous(), end->tokAt(1));
                    }

                    // Remove "if( true )"
                    if (tok->previous())
                        tok = tok->previous();
                    else
                        tok->str(";");

                    Token::eraseTokens(tok, tok->tokAt(5));
                }

                ret = true;
            }
        }

        // Handle if without else
        else
        {
            if (boolValue == false)
            {
                // Remove if and its content
                if (tok->previous())
                    tok = tok->previous();
                else
                    tok->str(";");

                Token::eraseTokens(tok, elseTag);
            }
            else
            {
                // convert "if( true ) {aaa;}" => "{aaa;}"
                if (tok->previous())
                    tok = tok->previous();
                else
                    tok->str(";");

                Token::eraseTokens(tok, tok->tokAt(5));
            }

            ret = true;
        }
    }

    return ret;
}


void Tokenizer::simplifyIfAddBraces()
{
    for (Token *tok = _tokens; tok; tok = tok ? tok->next() : NULL)
    {
        if (tok->previous() && !Token::Match(tok->previous(), ";|{|}|else|)|:"))
            continue;

        if (Token::Match(tok, "if|for|while ("))
        {
            // don't add "{}" around ";" in "do {} while();" (#609)
            const Token *prev = tok->previous();
            if (Token::simpleMatch(prev, "} while") &&
                prev->link() &&
                prev->link()->previous() &&
                prev->link()->previous()->str() == "do")
            {
                continue;
            }

            // Goto the ending ')'
            tok = tok->next()->link();

            // ')' should be followed by '{'
            if (!tok || Token::simpleMatch(tok, ") {"))
                continue;
        }

        else if (tok->str() == "else")
        {
            // An else followed by an if or brace don't need to be processed further
            if (Token::Match(tok, "else if|{"))
                continue;
        }

        else
        {
            continue;
        }

        // If there is no code after he if(), abort
        if (!tok->next())
            return;


        // insert open brace..
        tok->insertToken("{");
        tok = tok->next();
        Token *tempToken = tok;

        bool innerIf = Token::simpleMatch(tempToken->next(), "if");

        if (Token::simpleMatch(tempToken->next(), "do {"))
            tempToken = tempToken->tokAt(2)->link();

        // insert close brace..
        // In most cases it would work to just search for the next ';' and insert a closing brace after it.
        // But here are special cases..
        // * if (cond) for (;;) break;
        // * if (cond1) if (cond2) { }
        // * if (cond1) if (cond2) ; else ;
        int parlevel = 0;
        int indentlevel = 0;
        while ((tempToken = tempToken->next()) != NULL)
        {
            if (tempToken->str() == "{")
                ++indentlevel;

            else if (tempToken->str() == "}")
            {
                --indentlevel;
                if (indentlevel == 0 && parlevel == 0)
                    break;

                else if (indentlevel < 0 && parlevel == 0)
                {
                    // insert closing brace before this
                    tempToken = tempToken->previous();
                    break;
                }
            }

            else if (tempToken->str() == "(")
                ++parlevel;

            else if (tempToken->str() == ")")
            {
                if (parlevel == 0)
                {
                    tok->deleteThis();
                    tempToken = 0;
                    break;
                }
                --parlevel;
            }

            else if (indentlevel == 0 && parlevel == 0 && tempToken->str() == ";")
            {
                if (!innerIf)
                    break;

                if (Token::Match(tempToken, "; else if"))
                    ;
                else if (Token::Match(tempToken, "; else"))
                    innerIf = false;
                else
                    break;
            }
        }

        if (tempToken)
        {
            tempToken->insertToken("}");
            Token::createMutualLinks(tok, tempToken->next());
        }
    }
}

bool Tokenizer::simplifyDoWhileAddBracesHelper(Token *tok)
{
    if (Token::Match(tok->next(), "[),]"))
    {
        // fix for #988
        return false;
    }

    Token *tok1 = tok;  // token with "do"
    Token *tok2 = NULL; // token with "while"
    Token *tok3 = tok->next();

    // skip loop body
    bool result = false;
    while (tok3)
    {
        if (tok3->str() == "{")
        {
            tok3 = tok3->link();
        }
        else if (tok3->str() == "while")
        {
            tok2 = tok3;
            break;
        }
        else if (Token::Match(tok3, "do {"))
        {
            // Skip do{}while inside the current "do"
            tok3 = tok3->next()->link();
            if (Token::simpleMatch(tok3->next(), "while"))
                tok3 = tok3->next();
        }
        else if (Token::Match(tok3, "do !!{") &&
                 !Token::Match(tok3->next(), "[),]"))
        {
            // Handle do-while inside the current "do"
            // first and return true to get the outer
            // "do" to be handled later.
            tok1 = tok3;
            result = true;
        }

        tok3 = tok3->next();
    }

    if (tok2)
    {
        // insert "{" after "do"
        tok1->insertToken("{");

        // insert "}" before "while"
        tok2->previous()->insertToken("}");

        Token::createMutualLinks(tok1->next(), tok2->previous());
    }
    else
        result = false;

    return result;
}

void Tokenizer::simplifyDoWhileAddBraces()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "do !!{"))
        {
            while (simplifyDoWhileAddBracesHelper(tok))
            {
                // Call until the function returns false to
                // handle do-while inside do-while

            }
        }
    }
}

void Tokenizer::simplifyConditionOperator()
{
    int parlevel = 0;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() == "(")
            ++parlevel;
        else if (tok->str() == ")")
            --parlevel;
        else if (parlevel == 0 && Token::Match(tok, "; %var% = %var% ? %var% : %var% ;"))
        {
            const std::string var(tok->strAt(1));
            const std::string condition(tok->strAt(3));
            const std::string value1(tok->strAt(5));
            const std::string value2(tok->strAt(7));

            Token::eraseTokens(tok, tok->tokAt(9));

            std::string str("if ( " + condition + " ) { " + var + " = " + value1 + " ; } else { " + var + " = " + value2 + " ; }");
            std::string::size_type pos1 = 0;
            while (pos1 != std::string::npos)
            {
                std::string::size_type pos2 = str.find(" ", pos1);
                if (pos2 == std::string::npos)
                {
                    tok->insertToken(str.substr(pos1).c_str());
                    pos1 = pos2;
                }
                else
                {
                    tok->insertToken(str.substr(pos1, pos2 - pos1).c_str());
                    pos1 = pos2 + 1;
                }
                tok = tok->next();
            }

            Token::createMutualLinks(tok->tokAt(-15), tok->tokAt(-13));
            Token::createMutualLinks(tok->tokAt(-12), tok->tokAt(-7));
            Token::createMutualLinks(tok->tokAt(-5), tok);
        }
    }
}

bool Tokenizer::simplifyConditions()
{
    bool ret = false;

    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "! %num%") || Token::Match(tok, "! %bool%"))
        {
            if (tok->next()->str() == "0" || tok->next()->str() == "false")
                tok->str("true");
            else
                tok->str("false");

            tok->deleteNext();
            ret = true;
        }

        if (Token::simpleMatch(tok, "( true &&") ||
            Token::simpleMatch(tok, "&& true &&") ||
            Token::simpleMatch(tok->next(), "&& true )"))
        {
            Token::eraseTokens(tok, tok->tokAt(3));
            ret = true;
        }

        else if (Token::simpleMatch(tok, "( false ||") ||
                 Token::simpleMatch(tok, "|| false ||") ||
                 Token::simpleMatch(tok->next(), "|| false )"))
        {
            Token::eraseTokens(tok, tok->tokAt(3));
            ret = true;
        }

        // Change numeric constant in condition to "true" or "false"
        if (Token::Match(tok, "if|while ( %num%") &&
            (tok->tokAt(3)->str() == ")" || tok->tokAt(3)->str() == "||" || tok->tokAt(3)->str() == "&&"))
        {
            tok->tokAt(2)->str((tok->tokAt(2)->str() != "0") ? "true" : "false");
            ret = true;
        }
        Token *tok2 = tok->tokAt(2);
        if (tok2                                        &&
            (tok->str() == "&&" || tok->str() == "||")  &&
            Token::Match(tok->next(), "%num%")          &&
            (tok2->str() == ")" || tok2->str() == "&&" || tok2->str() == "||"))
        {
            tok->next()->str((tok->next()->str() != "0") ? "true" : "false");
            ret = true;
        }

        // Reduce "(%num% == %num%)" => "(true)"/"(false)"
        const Token *tok4 = tok->tokAt(4);
        if (! tok4)
            break;
        if ((tok->str() == "&&" || tok->str() == "||" || tok->str() == "(") &&
            (Token::Match(tok->tokAt(1), "%num% %any% %num%") ||
             Token::Match(tok->tokAt(1), "%bool% %any% %bool%")) &&
            (tok4->str() == "&&" || tok4->str() == "||" || tok4->str() == ")" || tok4->str() == "?"))
        {
            std::string cmp = tok->strAt(2);
            bool result = false;
            if (Token::Match(tok->tokAt(1), "%num%"))
            {
                // Compare numbers
                double op1 = MathLib::toDoubleNumber(tok->strAt(1));
                double op2 = MathLib::toDoubleNumber(tok->strAt(3));

                if (cmp == "==")
                    result = (op1 == op2);
                else if (cmp == "!=")
                    result = (op1 != op2);
                else if (cmp == ">=")
                    result = (op1 >= op2);
                else if (cmp == ">")
                    result = (op1 > op2);
                else if (cmp == "<=")
                    result = (op1 <= op2);
                else if (cmp == "<")
                    result = (op1 < op2);
                else
                    cmp = "";
            }
            else
            {
                // Compare boolean
                bool op1 = (tok->strAt(1) == std::string("true"));
                bool op2 = (tok->strAt(3) == std::string("true"));

                if (cmp == "==")
                    result = (op1 == op2);
                else if (cmp == "!=")
                    result = (op1 != op2);
                else if (cmp == ">=")
                    result = (op1 >= op2);
                else if (cmp == ">")
                    result = (op1 > op2);
                else if (cmp == "<=")
                    result = (op1 <= op2);
                else if (cmp == "<")
                    result = (op1 < op2);
                else
                    cmp = "";
            }

            if (! cmp.empty())
            {
                tok = tok->next();
                tok->deleteNext();
                tok->deleteNext();

                tok->str(result ? "true" : "false");
                ret = true;
            }
        }
    }

    return ret;
}

bool Tokenizer::simplifyQuestionMark()
{
    bool ret = false;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() != "?")
            continue;

        if (!tok->tokAt(-2))
            continue;

        if (!Token::Match(tok->tokAt(-2), "[=,(]"))
            continue;

        if (!Token::Match(tok->previous(), "%bool%") &&
            !Token::Match(tok->previous(), "%num%"))
            continue;

        // Find the ":" token..
        Token *semicolon = 0;
        {
            unsigned int parlevel = 0;
            for (Token *tok2 = tok; tok2; tok2 = tok2->next())
            {
                if (tok2->str() == "(")
                    ++parlevel;
                else if (tok2->str() == ")")
                {
                    if (parlevel == 0)
                        break;
                    --parlevel;
                }
                else if (parlevel == 0 && tok2->str() == ":")
                {
                    semicolon = tok2;
                    break;
                }
            }
        }
        if (!semicolon || !semicolon->next())
            continue;

        if (tok->previous()->str() == "false" ||
            tok->previous()->str() == "0")
        {
            // Use code after semicolon, remove code before it.
            semicolon = semicolon->next();
            tok = tok->tokAt(-2);
            Token::eraseTokens(tok, semicolon);

            tok = tok->next();
            ret = true;
        }

        // The condition is true. Delete the operator after the ":"..
        else
        {
            const Token *end = 0;

            // check the operator after the :
            if (Token::simpleMatch(semicolon, ": ("))
            {
                end = semicolon->next()->link();
                if (!Token::Match(end, ") !!."))
                    continue;
            }

            // delete the condition token and the "?"
            tok = tok->tokAt(-2);
            Token::eraseTokens(tok, tok->tokAt(3));

            // delete operator after the :
            if (end)
            {
                Token::eraseTokens(semicolon->previous(), end->next());
                continue;
            }

            int ind = 0;
            for (const Token *endTok = semicolon; endTok; endTok = endTok->next())
            {
                if (endTok->str() == ";")
                {
                    Token::eraseTokens(semicolon->previous(), endTok->next());
                    ret = true;
                    break;
                }

                else if (Token::Match(endTok, "[({[]"))
                {
                    ++ind;
                }

                else if (Token::Match(endTok, "[)}]]"))
                {
                    --ind;
                    if (ind < 0)
                    {
                        Token::eraseTokens(semicolon->previous(), endTok);
                        ret = true;
                        break;
                    }
                }
            }
        }
    }

    return ret;
}

void Tokenizer::simplifyCasts()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        while (Token::Match(tok->next(), "( %type% *| ) *|&| %var%") ||
               Token::Match(tok->next(), "( %type% %type% *| ) *|&| %var%"))
        {
            if (tok->isName() && tok->str() != "return")
                break;

            if (Token::simpleMatch(tok->previous(), "operator"))
                break;

            // Remove cast..
            Token::eraseTokens(tok, tok->next()->link()->next());

            if (tok->str() == ")" && tok->link()->previous())
            {
                // If there was another cast before this, go back
                // there to check it also. e.g. "(int)(char)x"
                tok = tok->link()->previous();
            }
        }

        // Replace pointer casts of 0.. "(char *)0" => "0"
        while (Token::Match(tok->next(), "( %type% * ) 0") ||
               Token::Match(tok->next(), "( %type% %type% * ) 0"))
        {
            Token::eraseTokens(tok, tok->next()->link()->next());
            if (tok->str() == ")" && tok->link()->previous())
            {
                // If there was another cast before this, go back
                // there to check it also. e.g. "(char*)(char*)0"
                tok = tok->link()->previous();
            }
        }

        if (Token::Match(tok->next(), "dynamic_cast|reinterpret_cast|const_cast|static_cast <"))
        {
            Token *tok2 = tok->next();
            unsigned int level = 0;
            while (tok2)
            {
                if (tok2->str() == "<")
                    ++level;
                else if (tok2->str() == ">")
                {
                    --level;
                    if (level == 0)
                        break;
                }
                tok2 = tok2->next();
            }

            if (Token::simpleMatch(tok2, "> ("))
            {
                Token *closeBracket = tok2->next()->link();
                if (closeBracket)
                {
                    Token::eraseTokens(tok, tok2->tokAt(2));
                    closeBracket->deleteThis();
                }
            }
        }
    }
}


void Tokenizer::simplifyFunctionParameters()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() == "{" || tok->str() == "[" || tok->str() == "(")
        {
            tok = tok->link();
            if (!tok)
                break;
            continue;
        }

        // Find the function e.g. foo( x ) or foo( x, y )
        if (Token::Match(tok, "%var% ( %var% [,)]"))
        {
            // We have found old style function, now we need to change it

            // backup pointer to the '(' token
            Token * const tok1 = tok->next();

            // Get list of argument names
            std::map<std::string, Token*> argumentNames;
            bool bailOut = false;
            for (tok = tok->tokAt(2); tok; tok = tok->tokAt(2))
            {
                if (!Token::Match(tok, "%var% [,)]"))
                {
                    bailOut = true;
                    break;
                }

                if (argumentNames.find(tok->str()) != argumentNames.end())
                {
                    // Invalid code, two arguments with the same name.
                    // TODO, print error perhaps?
                    bailOut = true;
                    break;
                }

                argumentNames[tok->str()] = tok;
                if (tok->next()->str() == ")")
                {
                    tok = tok->tokAt(2);
                    break;
                }
            }

            if (bailOut)
            {
                tok = tok1->link();
                if (!tok)
                    return;
                continue;
            }

            Token *start = tok;
            while (tok && tok->str() != "{")
            {
                if (tok->str() == ";")
                {
                    tok = tok->previous();
                    // Move tokens from start to tok into the place of
                    // argumentNames[tok->str()] and remove the ";"

                    if (argumentNames.find(tok->str()) == argumentNames.end())
                    {
                        bailOut = true;
                        break;
                    }

                    // Remove the following ";"
                    Token *temp = tok->tokAt(2);
                    tok->deleteNext();

                    // Replace "x" with "int x" or similar
                    Token::replace(argumentNames[tok->str()], start, tok);
                    argumentNames.erase(tok->str());
                    tok = temp;
                    start = tok;
                }
                else
                {
                    tok = tok->next();
                }
            }

            if (Token::simpleMatch(tok, "{"))
                tok = tok->link();

            if (tok == NULL)
            {
                break;
            }

            if (bailOut)
            {
                continue;
            }
        }
    }
}


void Tokenizer:: simplifyFunctionPointers()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->previous() && !Token::Match(tok->previous(), "[{};]"))
            continue;

        if (Token::Match(tok, "%type% *| *| ( * %var% ) ("))
            ;
        else if (Token::Match(tok, "%type% %type% *| *| ( * %var% ) ("))
            tok = tok->next();
        else
            continue;

        while (tok->next()->str() == "*")
            tok = tok->next();

        // check that the declaration ends with ;
        if (!Token::simpleMatch(tok->tokAt(5)->link(), ") ;"))
            continue;

        // ok simplify this function pointer to an ordinary pointer
        tok->deleteNext();
        tok->tokAt(2)->deleteNext();
        const Token *tok2 = tok->tokAt(3)->link();
        Token::eraseTokens(tok->tokAt(2), tok2 ? tok2->next() : 0);
    }
}


bool Tokenizer::simplifyFunctionReturn()
{
    bool ret = false;
    int indentlevel = 0;
    for (const Token *tok = tokens(); tok; tok = tok->next())
    {
        if (tok->str() == "{")
            ++indentlevel;

        else if (tok->str() == "}")
            --indentlevel;

        else if (indentlevel == 0 && Token::Match(tok, "%var% ( ) { return %num% ; }"))
        {
            std::ostringstream pattern;
            pattern << "[(=+-*/] " << tok->str() << " ( ) [;)+-*/]";
            for (Token *tok2 = _tokens; tok2; tok2 = tok2->next())
            {
                if (Token::Match(tok2, pattern.str().c_str()))
                {
                    tok2 = tok2->next();
                    tok2->str(tok->strAt(5));
                    tok2->deleteNext();
                    tok2->deleteNext();
                    ret = true;
                }
            }
        }
    }

    return ret;
}


static void incdec(std::string &value, const std::string &op)
{
    int ivalue = 0;
    std::istringstream istr(value.c_str());
    istr >> ivalue;
    if (op == "++")
        ++ivalue;
    else if (op == "--")
        --ivalue;
    std::ostringstream ostr;
    ostr << ivalue;
    value = ostr.str();
}



void Tokenizer::simplifyVarDecl()
{
    // Split up variable declarations..
    // "int a=4;" => "int a; a=4;"
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::simpleMatch(tok, "= {"))
        {
            tok = tok->next()->link();
            if (!tok)
                break;
        }

        if (tok->previous() && !Token::Match(tok->previous(), "[{};)]"))
            continue;

        Token *type0 = tok;
        if (!Token::Match(type0, "%type%"))
            continue;
        if (Token::Match(type0, "else|return"))
            continue;

        bool isconst = false;
        bool isstatic = false;
        Token *tok2 = type0;
        unsigned int typelen = 1;

        while (Token::Match(tok2, "%type% %type% *| *| %var%"))
        {
            if (tok2->str() == "const")
                isconst = true;

            else if (tok2->str() == "static")
                isstatic = true;

            tok2 = tok2->next();
            ++typelen;
        }

        // Don't split up const declaration..
        if (isconst && Token::Match(tok2, "%type% %var% ="))
            continue;

        // strange looking variable declaration => don't split up.
        if (Token::Match(tok2, "%type% *| %var% , %type% *| %var%"))
            continue;

        if (Token::Match(tok2, "%type% *| %var% ,|="))
        {
            const bool isPointer = (tok2->next()->str() == "*");
            const Token *varName = tok2->tokAt((isPointer ? 2 : 1));
            Token *endDeclaration = varName->next();

            if (varName->str() != "operator")
            {
                tok2 = endDeclaration; // The ',' or '=' token

                if (isstatic && tok2->str() == "=")
                {
                    if (Token::Match(tok2->next(), "%num% ,"))
                        tok2 = tok2->tokAt(2);
                    else
                        tok2 = NULL;
                }
            }
            else
                tok2 = NULL;
        }

        else if (Token::Match(tok2, "%type% * * %var% ,|="))
        {
            if (tok2->tokAt(3)->str() != "operator")
                tok2 = tok2->tokAt(4);    // The ',' token
            else
                tok2 = NULL;
        }

        else if (Token::Match(tok2, "%type% * const %var% ,|="))
        {
            if (tok2->tokAt(3)->str() != "operator")
            {
                tok2 = tok2->tokAt(4);    // The ',' token
            }
            else
            {
                tok2 = NULL;
            }
        }

        else if (Token::Match(tok2, "%type% %var% [ %num% ] ,|=") ||
                 Token::Match(tok2, "%type% %var% [ %var% ] ,|="))
        {
            tok2 = tok2->tokAt(5);    // The ',' token

            if (tok2->str() == "=")
            {
                while (tok2 && tok2->str() != ",")
                {
                    if (tok2->str() == "{")
                        tok2 = tok2->link();

                    tok2 = tok2->next();

                    if (tok2->str() == ";")
                        tok2 = NULL;
                }
            }
        }

        else if (Token::Match(tok2, "%type% * %var% [ %num% ] ,") ||
                 Token::Match(tok2, "%type% * %var% [ %var% ] ,"))
        {
            tok2 = tok2->tokAt(6);    // The ',' token
        }

        else if (Token::Match(tok2, "std :: %type% <") || Token::Match(tok2, "%type% <"))
        {
            //
            // Deal with templates and standart types
            //
            if (Token::simpleMatch(tok2, "std ::"))
            {
                typelen += 2;
                tok2 = tok2->tokAt(2);
            }

            typelen += 2;
            tok2 = tok2->tokAt(2);
            size_t indentlevel = 1;

            for (Token *tok3 = tok2; tok3; tok3 = tok3->next())
            {
                ++typelen;

                if (tok3->str() == "<")
                {
                    ++indentlevel;
                }
                else if (tok3->str() == ">")
                {
                    --indentlevel;
                    if (indentlevel == 0)
                    {
                        tok2 = tok3->next();
                        break;
                    }
                }
                else if (tok3->str() == ";")
                {
                    break;
                }
            }

            if (Token::Match(tok2, ":: %type%"))
            {
                typelen += 2;
                tok2 = tok2->tokAt(2);
            }

            if (tok2->str() == "*" || tok2->str() == "&")
            {
                tok2 = tok2->next();
            }

            if (Token::Match(tok2, "%var% ,"))
            {
                tok2 = tok2->next();    // The ',' token
                typelen--;
            }
            else
            {
                tok2 = NULL;
                typelen = 0;
            }
        }
        else
        {
            tok2 = NULL;
            typelen = 0;
        }


        if (tok2)
        {
            if (tok2->str() == ",")
            {
                tok2->str(";");
                insertTokens(tok2, type0, typelen);
                std::stack<Token *> link1;
                std::stack<Token *> link2;
                while (((typelen--) > 0) && (0 != (tok2 = tok2->next())))
                {
                    if (tok2->str() == "(")
                        link1.push(tok2);
                    else if (tok2->str() == ")" && !link1.empty())
                    {
                        Token::createMutualLinks(tok2, link1.top());
                        link1.pop();
                    }

                    else if (tok2->str() == "[")
                        link2.push(tok2);
                    else if (tok2->str() == "]" && !link2.empty())
                    {
                        Token::createMutualLinks(tok2, link2.top());
                        link2.pop();
                    }
                }
            }

            else
            {
                Token *eq = tok2;

                int parlevel = 0;
                while (tok2)
                {
                    if (Token::Match(tok2, "[{(<]"))
                    {
                        ++parlevel;
                    }

                    else if (Token::Match(tok2, "[})>]"))
                    {
                        if (parlevel <= 0)
                            break;
                        --parlevel;
                    }

                    else if (parlevel == 0 && strchr(";,", tok2->str()[0]))
                    {
                        // "type var ="   =>   "type var; var ="
                        Token *VarTok = type0->tokAt(typelen);
                        while (Token::Match(VarTok, "*|const"))
                            VarTok = VarTok->next();
                        insertTokens(eq, VarTok, 2);
                        eq->str(";");

                        // "= x, "   =>   "= x; type "
                        if (tok2->str() == ",")
                        {
                            tok2->str(";");
                            insertTokens(tok2, type0, typelen);
                        }
                        break;
                    }

                    tok2 = tok2->next();
                }
            }
        }
    }
}

void Tokenizer::simplifyStdType()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        // long unsigned => unsigned long
        if (Token::Match(tok, "long|short|int|char|_int64 unsigned|signed"))
        {
            std::string temp = tok->str();
            tok->str(tok->next()->str());
            tok->next()->str(temp);
        }

        if (!Token::Match(tok, "unsigned|signed|long|char|short|int|__int64"))
            continue;

        // check if signed or unsigned specified
        if (Token::Match(tok, "unsigned|signed"))
        {
            bool isUnsigned = tok->str() == "unsigned";

            // unsigned i => unsigned int i
            if (!tok->next()->isIntegerType())
                tok->str("int");
            else
                tok->deleteThis();
            tok->isUnsigned(isUnsigned);
            tok->isSigned(!isUnsigned);
        }

        if (Token::Match(tok, "__int64"))
        {
            tok->str("long");
            tok->isLong(true);
        }
        else if (Token::Match(tok, "long"))
        {
            if (Token::Match(tok->next(), "long"))
            {
                tok->isLong(true);
                tok->deleteNext();
            }

            if (Token::Match(tok->next(), "int"))
                tok->deleteNext();
            else if (Token::Match(tok->next(), "double"))
            {
                tok->str("double");
                tok->isLong(true);
                tok->deleteNext();
            }
        }
        else if (Token::Match(tok, "short"))
        {
            if (Token::Match(tok->next(), "int"))
                tok->deleteNext();
        }
    }
}

void Tokenizer::simplifyIfAssign()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (!Token::Match(tok->next(), "if|while ( !| (| %var% =") &&
            !Token::Match(tok->next(), "if|while ( !| (| %var% . %var% ="))
            continue;

        // simplifying a "while" condition ?
        const bool iswhile(tok->next()->str() == "while");

        // delete the "if"
        tok->deleteNext();

        // Remember if there is a "!" or not. And delete it if there are.
        const bool isNot(tok->tokAt(2)->str() == "!");
        if (isNot)
            tok->next()->deleteNext();

        // Delete paranthesis.. and remember how many there are with
        // their links.
        std::list<Token *> braces;
        while (tok->next()->str() == "(")
        {
            braces.push_back(tok->next()->link());
            tok->deleteNext();
        }

        // Skip the "%var% = ..."
        Token *tok2;
        unsigned int indentlevel = 0;
        for (tok2 = tok->next(); tok2; tok2 = tok2->next())
        {
            if (tok2->str() == "(")
                ++indentlevel;
            else if (tok2->str() == ")")
            {
                if (indentlevel <= 0)
                    break;
                --indentlevel;
            }
        }

        // Insert "; if|while ( .."
        tok2 = tok2->previous();
        if (Token::simpleMatch(tok->tokAt(2), "."))
        {
            tok2->insertToken(tok->strAt(3));
            tok2->insertToken(tok->strAt(2));
        }
        tok2->insertToken(tok->strAt(1));
        tok2->next()->varId(tok->tokAt(1)->varId());

        while (! braces.empty())
        {
            tok2->insertToken("(");
            Token::createMutualLinks(tok2->next(), braces.back());
            braces.pop_back();
        }

        if (isNot)
            tok2->next()->insertToken("!");
        tok2->insertToken(iswhile ? "while" : "if");
        tok2->insertToken(";");

        // If it's a while loop.. insert the assignment in the loop
        if (iswhile)
        {
            indentlevel = 0;
            Token *tok3 = tok2;
            for (tok3 = tok2; tok3; tok3 = tok3->next())
            {
                if (tok3->str() == "{")
                    ++indentlevel;
                else if (tok3->str() == "}")
                {
                    if (indentlevel <= 1)
                        break;
                    --indentlevel;
                }
            }

            if (tok3 && indentlevel == 1)
            {
                tok3 = tok3->previous();
                std::list<Token *> braces2;

                for (tok2 = tok2->next(); tok2 && tok2 != tok; tok2 = tok2->previous())
                {
                    tok3->insertToken(tok2->strAt(0));

                    Token *newTok = tok3->next();
                    newTok->fileIndex(tok2->fileIndex());
                    newTok->linenr(tok2->linenr());

                    // link() newly tokens manually
                    if (newTok->str() == ")")
                    {
                        braces.push_back(newTok);
                    }
                    else if (newTok->str() == "]")
                    {
                        braces2.push_back(newTok);
                    }
                    else if (newTok->str() == "(")
                    {
                        Token::createMutualLinks(newTok, braces.back());
                        braces.pop_back();
                    }
                    else if (newTok->str() == "[")
                    {
                        Token::createMutualLinks(newTok, braces2.back());
                        braces2.pop_back();
                    }
                }
            }
        }
    }
}


void Tokenizer::simplifyVariableMultipleAssign()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "%var% = %var% = %num% ;") ||
            Token::Match(tok, "%var% = %var% = %var% ;"))
        {

            // skip intermediate assignments
            Token *tok2 = tok->previous();
            while (tok2 &&
                   tok2->str() == "=" &&
                   Token::Match(tok2->previous(), "%var%"))
            {
                tok2 = tok2->tokAt(-2);
            }

            if (tok2->str() != ";")
            {
                continue;
            }

            Token *stopAt = tok->tokAt(2);
            const Token *valueTok = tok->tokAt(4);
            const std::string value(valueTok->str());
            tok2 = tok2->next();

            while (tok2 != stopAt)
            {
                tok2->next()->insertToken(";");
                tok2->next()->insertToken(value);
                tok2 = tok2->tokAt(4);
            }
        }
    }
}


void Tokenizer::simplifyIfNot()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() == "(" || tok->str() == "||" || tok->str() == "&&")
        {
            tok = tok->next();
            while (tok && tok->str() == "(")
                tok = tok->next();

            if (!tok)
                break;

            if (Token::Match(tok, "0|false == (") ||
                Token::Match(tok, "0|false == %var%"))
            {
                tok->deleteNext();
                tok->str("!");
            }

            else if (Token::Match(tok, "%var% == 0|false"))
            {
                tok->deleteNext();
                tok->next()->str(tok->str());
                tok->str("!");
            }

            else if (Token::Match(tok, "%var% .|:: %var% == 0|false"))
            {
                tok = tok->previous();
                tok->insertToken("!");
                tok = tok->tokAt(4);
                Token::eraseTokens(tok, tok->tokAt(3));
            }

            else if (Token::Match(tok, "* %var% == 0|false"))
            {
                tok = tok->previous();
                tok->insertToken("!");
                tok = tok->tokAt(3);
                Token::eraseTokens(tok, tok->tokAt(3));
            }
        }

        else if (tok->link() && Token::Match(tok, ") == 0|false"))
        {
            Token::eraseTokens(tok, tok->tokAt(3));
            if (Token::Match(tok->link()->previous(), "%var%"))
            {
                // if( foo(x) == 0 )
                tok->link()->previous()->insertToken(tok->link()->previous()->str().c_str());
                tok->link()->previous()->previous()->str("!");
            }
            else
            {
                // if( (x) == 0 )
                tok->link()->insertToken("(");
                tok->link()->str("!");
                Token *temp = tok->link();
                Token::createMutualLinks(tok->link()->next(), tok);
                temp->link(0);
            }
        }
    }
}


void Tokenizer::simplifyIfNotNull()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        Token *deleteFrom = NULL;

        if (tok->str() == "(" || tok->str() == "||" || tok->str() == "&&")
        {
            tok = tok->next();

            if (Token::simpleMatch(tok, "0 != (") ||
                Token::Match(tok, "0 != %var%"))
            {
                deleteFrom = tok->previous();
            }

            else if (Token::Match(tok, "%var% != 0"))
            {
                deleteFrom = tok;
            }

            else if (Token::Match(tok, "%var% .|:: %var% != 0"))
            {
                tok = tok->tokAt(2);
                deleteFrom = tok;
            }
        }

        else if (tok->link() && Token::simpleMatch(tok, ") != 0"))
        {
            deleteFrom = tok;
        }

        if (deleteFrom)
        {
            Token::eraseTokens(deleteFrom, deleteFrom->tokAt(3));
            tok = deleteFrom;
        }
    }
}


void Tokenizer::simplifyIfSameInnerCondition()
{
    // same inner condition
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "if ( %var% ) {"))
        {
            const unsigned int varid(tok->tokAt(2)->varId());
            if (!varid)
                continue;

            for (Token *tok2 = tok->tokAt(5); tok2; tok2 = tok2->next())
            {
                if (tok2->str() == "{" || tok2->str() == "}")
                    break;
                if (Token::simpleMatch(tok2, "if ("))
                {
                    tok2 = tok2->tokAt(2);
                    if (Token::Match(tok2, "%varid% )", varid))
                        tok2->str("true");
                    else if (Token::Match(tok2, "! %varid% )", varid))
                        tok2->next()->varId(varid);
                    break;
                }
            }
        }
    }
}


void Tokenizer::simplifyLogicalOperators()
{
    // "if (not p)" => "if (!p)"
    // "if (p and q)" => "if (p and q)"
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "if|while ( not %var%"))
        {
            tok->tokAt(2)->str("!");
        }
        else if (Token::Match(tok, "&& not %var%"))
        {
            tok->next()->str("!");
        }
        else if (Token::Match(tok, "|| not %var%"))
        {
            tok->next()->str("!");
        }
        // "%var%|) and %var%|("
        else if (tok->str() == "and" &&
                 ((Token::Match(tok->previous(), "%var%") || tok->previous()->str() == ")") ||
                  (Token::Match(tok->next(), "%var%") || tok->next()->str() == "(")))
        {
            tok->str("&&");
        }
    }
}

// int i(0); => int i; i = 0;
void Tokenizer::simplifyInitVar()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "[{};] class|struct|union| %type% *| %var% ( &| %any% ) ;"))
            tok = initVar(tok->next());
        else if (tok == _tokens && Token::Match(tok, "class|struct|union| %type% *| %var% ( &| %any% ) ;"))
            tok = initVar(tok);
    }
}

Token * Tokenizer::initVar(Token * tok)
{
    // call constructor of class => no simplification
    if (Token::Match(tok, "class|struct|union"))
    {
        if (tok->tokAt(2)->str() != "*")
            return tok;

        tok = tok->next();
    }
    else if (!tok->isStandardType() && tok->tokAt(1)->str() != "*")
        return tok;

    // goto variable name..
    tok = tok->next();
    if (tok->str() == "*")
        tok = tok->next();

    // check initializer..
    if (tok->tokAt(2)->isStandardType() || tok->tokAt(2)->str() == "void")
        return tok;
    else if (!tok->tokAt(2)->isNumber() && tok->tokAt(2)->str() != "&" && tok->tokAt(2)->varId() == 0)
        return tok;

    // insert '; var ='
    tok->insertToken(";");
    tok->next()->insertToken(tok->str());
    tok = tok->tokAt(2);
    tok->insertToken("=");

    // remove parantheses..
    tok = tok->next();
    tok->deleteNext();
    tok = tok->next();
    if (tok->str() == "&")
        tok = tok->next();
    tok->deleteNext();

    return tok;
}


bool Tokenizer::simplifyKnownVariables()
{
    bool ret = false;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        // Search for a block of code
        if (! Token::Match(tok, ") const| {"))
            continue;

        // parse the block of code..
        int indentlevel = 0;
        Token *tok2 = tok;
        for (; tok2; tok2 = tok2->next())
        {

            if (tok2->str() == "{")
                ++indentlevel;

            else if (tok2->str() == "}")
            {
                --indentlevel;
                if (indentlevel <= 0)
                    break;
            }

            else if (tok2->previous()->str() != "*" &&
                     (Token::Match(tok2, "%var% = %num% ;") ||
                      Token::Match(tok2, "%var% = %str% ;") ||
                      Token::Match(tok2, "%var% [ ] = %str% ;") ||
                      Token::Match(tok2, "%var% = %bool% ;") ||
                      Token::Match(tok2, "%var% = %var% ;") ||
                      Token::Match(tok2, "%var% = & %var% ;") ||
                      Token::Match(tok2, "%var% = & %var% [ 0 ] ;")))
            {
                const unsigned int varid = tok2->varId();
                if (varid == 0)
                    continue;

                if (tok2->str() == tok2->strAt(2))
                    continue;

                const bool pointeralias(tok2->tokAt(2)->isName() || tok2->tokAt(2)->str() == "&");

                std::string value;

                Token *tok3 = NULL;

                if (Token::Match(tok2->tokAt(-2), "for ( %varid% = %num% ; %varid% <|<= %num% ; ++| %varid% ++| ) {", varid))
                {
                    const std::string compareop = tok2->strAt(5);
                    if (compareop == "<")
                        value = tok2->strAt(6);
                    else
                        value = MathLib::toString(MathLib::toLongNumber(tok2->strAt(6)) + 1);

                    // Skip for-body..
                    tok3 = tok2->previous()->link()->next()->link()->next();
                }
                else
                {
                    value = tok2->strAt(2);
                    if (value == "]")
                        value = tok2->strAt(4);
                    else if (value == "&")
                        value = tok2->strAt(3);
                    if (Token::simpleMatch(tok2->next(), "= &"))
                        tok2 = tok2->tokAt(3);
                    tok3 = tok2->next();
                }
                Token* bailOutFromLoop = 0;
                int indentlevel3 = indentlevel;     // indentlevel for tok3
                bool ret3 = false;
                for (; tok3; tok3 = tok3->next())
                {
                    if (tok3->str() == "{")
                    {
                        ++indentlevel3;
                    }
                    else if (tok3->str() == "}")
                    {
                        --indentlevel3;
                        if (indentlevel3 < indentlevel)
                        {
                            if (Token::Match(tok2->tokAt(-7), "%type% * %var% ; %var% = & %var% ;") &&
                                tok2->tokAt(-5)->str() == tok2->tokAt(-3)->str())
                            {
                                tok2 = tok2->tokAt(-4);
                                Token::eraseTokens(tok2, tok2->tokAt(5));
                            }
                            break;
                        }
                    }

                    // Stop if label is found
                    if (Token::Match(tok3, "; %type% : ;"))
                        break;

                    // Stop if return is found ..
                    if (indentlevel3 == 1)
                    {
                        if (tok3->str() == "return")
                            ret3 = true;
                        else if (tok3->str() == ";")
                        {
                            if (ret3 && !Token::simpleMatch(tok3->next(), "}"))
                                break;
                            ret3 = false;
                        }
                    }

                    if (pointeralias && Token::Match(tok3, ("!!= " + value).c_str()))
                        break;

                    // Stop if something like 'while (--var)' is found
                    if (tok3->str() == "while" || tok3->str() == "do")
                    {
                        const Token *endpar = tok3->next()->link();
                        bool bailout = false;
                        for (const Token *tok4 = tok3; tok4 && tok4 != endpar; tok4 = tok4->next())
                        {
                            if (Token::Match(tok4, "++|-- %varid%", varid) ||
                                Token::Match(tok4, "%varid% ++|--|=", varid))
                            {
                                bailout = true;
                                break;
                            }
                        }
                        if (bailout)
                            break;
                    }

                    if (bailOutFromLoop)
                    {
                        // This could be a loop, skip it, but only if it doesn't contain
                        // the variable we are checking for. If it contains the variable
                        // we will bail out.
                        if (tok3->varId() == varid)
                        {
                            // Continue
                            tok2 = bailOutFromLoop;
                            break;
                        }
                        else if (tok3 == bailOutFromLoop)
                        {
                            // We have skipped the loop
                            bailOutFromLoop = 0;
                            continue;
                        }

                        continue;
                    }
                    else if (tok3->str() == "{" && tok3->previous()->str() == ")")
                    {
                        // There is a possible loop after the assignment. Try to skip it.
                        if (tok3->previous()->link() &&
                            !Token::simpleMatch(tok3->previous()->link()->previous(), "if"))
                            bailOutFromLoop = tok3->link();
                        continue;
                    }
                    else if (tok3->str() == "}" && tok3->link() && tok3->link()->previous()->str() == ")")
                    {
                        // Assignment was in the middle of possible loop, bail out.
                        break;
                    }

                    // Variable is used somehow in a non-defined pattern => bail out
                    if (tok3->varId() == varid)
                        break;

                    // Using the variable in condition..
                    if (Token::Match(tok3->previous(), "if ( %varid% ==|!=|<|<=|>|>=|)", varid) ||
                        Token::Match(tok3, "( %varid% ==|!=|<|<=|>|>=", varid) ||
                        Token::Match(tok3, "!|==|!=|<|<=|>|>= %varid% ==|!=|<|<=|>|>=|)", varid) ||
                        Token::Match(tok3->previous(), "strlen|free ( %varid% )", varid))
                    {
                        tok3 = tok3->next();
                        tok3->str(value);
                        ret = true;
                    }

                    // Variable is used in calculation..
                    if (Token::Match(tok3, "[=+-*/[] %varid% [=?+-*/;]]", varid) ||
                        Token::Match(tok3, "[(=+-*/[] %varid% <<|>>", varid) ||
                        Token::Match(tok3, "<< %varid% [+-*/;])]", varid) ||
                        Token::Match(tok3, ">> %varid% [+-*/])]", varid))
                    {
                        tok3 = tok3->next();
                        tok3->str(value);
                        ret = true;
                    }

                    // Using the variable in for-condition..
                    if (Token::simpleMatch(tok3, "for ("))
                    {
                        for (Token *tok4 = tok3->tokAt(2); tok4; tok4 = tok4->next())
                        {
                            if (tok4->str() == "(" || tok4->str() == ")")
                                break;

                            // Replace variable used in condition..
                            if (Token::Match(tok4, "; %var% <|<=|!= %var% ; ++| %var% ++| )"))
                            {
                                const Token *inctok = tok4->tokAt(5);
                                if (inctok->str() == "++")
                                    inctok = inctok->next();
                                if (inctok->varId() == varid)
                                    break;

                                if (tok4->next()->varId() == varid)
                                {
                                    tok4->next()->str(value);
                                    ret = true;
                                }
                                if (tok4->tokAt(3)->varId() == varid)
                                {
                                    tok4->tokAt(3)->str(value);
                                    ret = true;
                                }
                            }
                        }

                        if (tok3->next()->varId() == varid)
                        {
                            tok3->next()->str(value);
                            ret = true;
                        }
                        else if (tok3->tokAt(3)->varId() == varid)
                        {
                            tok3->tokAt(3)->str(value);
                            ret = true;
                        }
                    }

                    if (indentlevel == indentlevel3 && Token::Match(tok3->next(), "%varid% ++|--", varid) && MathLib::isInt(value))
                    {
                        const std::string op(tok3->strAt(2));
                        if (Token::Match(tok3, "[{};] %any% %any% ;"))
                        {
                            Token::eraseTokens(tok3, tok3->tokAt(3));
                        }
                        else
                        {
                            tok3 = tok3->next();
                            tok3->str(value);
                            tok3->deleteNext();
                        }
                        incdec(value, op);
                        tok2->tokAt(2)->str(value);
                        ret = true;
                    }

                    if (indentlevel == indentlevel3 && Token::Match(tok3->next(), "++|-- %varid%", varid) && MathLib::isInt(value) &&
                        !Token::Match(tok3->tokAt(3), "[.[]"))
                    {
                        incdec(value, tok3->strAt(1));
                        tok2->tokAt(2)->str(value);
                        if (Token::Match(tok3, "[;{}] %any% %any% ;"))
                        {
                            Token::eraseTokens(tok3, tok3->tokAt(3));
                        }
                        else
                        {
                            tok3->deleteNext();
                            tok3->next()->str(value);
                        }
                        tok3 = tok3->next();
                        ret = true;
                    }

                    // return variable..
                    if (Token::Match(tok3, "return %varid% ;", varid))
                    {
                        tok3->next()->str(value);
                    }

                    else if (pointeralias && Token::Match(tok3, "return * %varid% ;", varid))
                    {
                        tok3->deleteNext();
                        tok3->next()->str(value);
                    }
                }
            }
        }

        if (tok2)
            tok = tok2->previous();
    }

    return ret;
}


void Tokenizer::elseif()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (!Token::simpleMatch(tok, "else if"))
            continue;
        int indent = 0;
        for (Token *tok2 = tok; indent >= 0 && tok2; tok2 = tok2->next())
        {
            if (Token::Match(tok2, "(|{"))
                ++indent;
            else if (Token::Match(tok2, ")|}"))
                --indent;

            if (indent == 0 && Token::Match(tok2, "}|;"))
            {
                if (tok2->next() && tok2->next()->str() != "else")
                {
                    tok->insertToken("{");
                    tok2->insertToken("}");
                    Token::createMutualLinks(tok->next(), tok2->next());
                    break;
                }
            }
        }
    }
}


bool Tokenizer::simplifyRedundantParanthesis()
{
    bool ret = false;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() != "(")
            continue;

        while (Token::simpleMatch(tok, "( (") &&
               tok->link()->previous() == tok->next()->link())
        {
            // We have "(( *something* ))", remove the inner
            // paranthesis
            tok->deleteNext();
            tok->link()->tokAt(-2)->deleteNext();
            ret = true;
        }

        while (Token::Match(tok->previous(), "[;{] ( %var% (") &&
               tok->link()->previous() == tok->tokAt(2)->link())
        {
            // We have "( func ( *something* ))", remove the outer
            // paranthesis
            tok->link()->deleteThis();
            tok->deleteThis();
            ret = true;
        }

        while (Token::Match(tok->previous(), "[;{] ( delete %var% ) ;"))
        {
            // We have "( delete var )", remove the outer
            // paranthesis
            tok->tokAt(3)->deleteThis();
            tok->deleteThis();
            ret = true;
        }

        while (Token::Match(tok->previous(), "[;{] ( delete [ ] %var% ) ;"))
        {
            // We have "( delete [] var )", remove the outer
            // paranthesis
            tok->tokAt(5)->deleteThis();
            tok->deleteThis();
            ret = true;
        }

        if ((Token::simpleMatch(tok->previous(), "delete (") && Token::Match(tok->link(), ") ;|,")) ||
            (Token::simpleMatch(tok->previous(), "; (") && Token::Match(tok->link(), ") ;")))
        {
            tok->link()->deleteThis();
            tok->deleteThis();
            ret = true;
        }

        if (Token::Match(tok->previous(), "[(!*;{}] ( %var% )") && tok->next()->varId() != 0)
        {
            // We have "( var )", remove the paranthesis
            tok->deleteThis();
            tok->deleteNext();
            ret = true;
            continue;
        }

        if (Token::Match(tok->previous(), "[(!] ( %var% . %var% )"))
        {
            // We have "( var . var )", remove the paranthesis
            tok->deleteThis();
            tok = tok->tokAt(2);
            tok->deleteNext();
            ret = true;
            continue;
        }

        if (Token::Match(tok, "( ( %bool% )") ||
            Token::Match(tok, "( ( %num% )"))
        {
            tok->tokAt(2)->deleteNext();
            tok->deleteNext();
            ret = true;
        }
    }
    return ret;
}

void Tokenizer::simplifyReference()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        // starting executable scope..
        if (Token::Match(tok, ") const| {"))
        {
            // replace references in this scope..
            if (tok->next()->str() != "{")
                tok = tok->next();
            Token * const end = tok->next()->link();
            for (Token *tok2 = tok; tok2 && tok2 != end; tok2 = tok2->next())
            {
                // found a reference..
                if (Token::Match(tok2, "[;{}] %type% & %var% (|= %var% )| ;"))
                {
                    const unsigned int ref_id = tok2->tokAt(3)->varId();
                    if (!ref_id)
                        continue;

                    // replace reference in the code..
                    for (Token *tok3 = tok2->tokAt(7); tok3 && tok3 != end; tok3 = tok3->next())
                    {
                        if (tok3->varId() == ref_id)
                        {
                            tok3->str(tok2->strAt(5));
                            tok3->varId(tok2->tokAt(5)->varId());
                        }
                    }

                    Token::eraseTokens(tok2, tok2->tokAt(7));
                }
            }
        }
    }
}

bool Tokenizer::simplifyCalculations()
{
    bool ret = false;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::simpleMatch(tok->next(), "* 1") || Token::simpleMatch(tok->next(), "1 *"))
        {
            Token::eraseTokens(tok, tok->tokAt(3));
            ret = true;
        }

        // (1-2)
        while (Token::Match(tok, "[[,(=<>+-*] %num% [+-*/] %num% [],);=<>+-*/]") ||
               Token::Match(tok, "<< %num% [+-*/] %num% [],);=<>+-*/]") ||
               Token::Match(tok, "[[,(=<>+-*] %num% [+-*/] %num% <<") ||
               Token::Match(tok, "<< %num% [+-*/] %num% <<"))
        {
            tok = tok->next();

            // Don't simplify "%num% / 0"
            if (Token::simpleMatch(tok->next(), "/ 0"))
                continue;

            // + and - are calculated after * and /
            if (Token::Match(tok->next(), "[+-/]"))
            {
                if (tok->previous()->str() == "*")
                    continue;
                if (Token::Match(tok->tokAt(3), "[*/]"))
                    continue;
            }

            if (Token::Match(tok->previous(), "- %num% - %num%"))
                tok->str(MathLib::calculate(tok->str(), tok->tokAt(2)->str(), '+'));
            else if (Token::Match(tok->previous(), "- %num% + %num%"))
                tok->str(MathLib::calculate(tok->str(), tok->tokAt(2)->str(), '-'));
            else
                tok->str(MathLib::calculate(tok->str(), tok->tokAt(2)->str(), tok->strAt(1)[0]));

            Token::eraseTokens(tok, tok->tokAt(3));

            // evaluate "2 + 2 - 2 - 2"
            // as (((2 + 2) - 2) - 2) = 0
            // instead of ((2 + 2) - (2 - 2)) = 4
            if (Token::Match(tok->next(), "[+-*/]"))
            {
                tok = tok->previous();
                continue;
            }

            ret = true;
        }

        // Remove parantheses around number..
        if (!tok->isName() && Token::Match(tok->next(), "( %num% )"))
        {
            tok->deleteNext();
            tok = tok->next();
            tok->deleteNext();
            ret = true;
        }

        // Remove parantheses around variable..
        // keep parantheses here: dynamic_cast<Fred *>(p);
        // keep parantheses here: A operator * (int);
        if (!tok->isName() && tok->str() != ">" && Token::Match(tok->next(), "( %var% ) [;),+-*/><]]") && !Token::simpleMatch(tok->previous(), "operator"))
        {
            tok->deleteNext();
            tok = tok->next();
            tok->deleteNext();
            ret = true;
        }

        if (Token::simpleMatch(tok->previous(), "( 0 ||") ||
            Token::simpleMatch(tok, "|| 0 )") ||
            Token::simpleMatch(tok->previous(), "( 1 &&") ||
            Token::simpleMatch(tok, "&& 1 )"))
        {
            tok->deleteThis();
            tok->deleteThis();
        }

        if (Token::Match(tok, "%num% ==|!=|<=|>=|<|> %num%") &&
            MathLib::isInt(tok->str()) &&
            MathLib::isInt(tok->tokAt(2)->str()))
        {
            const std::string prev(tok->previous() ? tok->strAt(-1).c_str() : "");
            const std::string after(tok->tokAt(3) ? tok->strAt(3).c_str() : "");
            if ((prev == "(" || prev == "&&" || prev == "||") && (after == ")" || after == "&&" || after == "||"))
            {
                const int op1(MathLib::toLongNumber(tok->str()));
                const std::string &cmp(tok->next()->str());
                const int op2(MathLib::toLongNumber(tok->tokAt(2)->str()));

                std::string result;

                if (cmp == "==")
                    result = (op1 == op2) ? "1" : "0";
                else if (cmp == "!=")
                    result = (op1 != op2) ? "1" : "0";
                else if (cmp == "<=")
                    result = (op1 <= op2) ? "1" : "0";
                else if (cmp == ">=")
                    result = (op1 >= op2) ? "1" : "0";
                else if (cmp == "<")
                    result = (op1 < op2) ? "1" : "0";
                else if (cmp == ">")
                    result = (op1 > op2) ? "1" : "0";

                tok->str(result);
                tok->deleteNext();
                tok->deleteNext();
            }
        }

        if (Token::Match(tok, "%num% <<|>> %num%"))
        {
            const int op1(MathLib::toLongNumber(tok->str()));
            const int op2(MathLib::toLongNumber(tok->tokAt(2)->str()));
            int result;

            if (tok->next()->str() == "<<")
                result = op1 << op2;
            else
                result = op1 >> op2;

            std::stringstream ss;
            ss << result;

            tok->str(ss.str());
            tok->deleteNext();
            tok->deleteNext();
        }
    }
    return ret;
}




void Tokenizer::simplifyGoto()
{
    std::list<Token *> gotos;
    unsigned int indentlevel = 0;
    Token *beginfunction = 0;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() == "{")
        {
            if (beginfunction == 0 && indentlevel == 0 && tok->link())
                tok = tok->link();
            else
                ++indentlevel;
        }

        else if (tok->str() == "}")
        {
            if (indentlevel == 0)
                break;  // break out - it seems the code is wrong
            --indentlevel;
            if (indentlevel == 0)
            {
                gotos.clear();
                beginfunction = 0;
            }
        }

        else if (indentlevel == 0 && Token::Match(tok, ") const| {"))
        {
            gotos.clear();
            beginfunction = tok;
        }

        else if (Token::Match(tok, "goto %var% ;"))
            gotos.push_back(tok);

        else if (indentlevel == 1 && Token::Match(tok->previous(), "[};] %var% :"))
        {
            // Is this label at the end..
            bool end = false;
            int level = 0;
            for (const Token *tok2 = tok->tokAt(2); tok2; tok2 = tok2->next())
            {
                if (tok2->str() == "}")
                {
                    --level;
                    if (level < 0)
                    {
                        end = true;
                        break;
                    }
                }
                else if (tok2->str() == "{")
                {
                    ++level;
                }

                if (Token::Match(tok2, "%var% :") || tok2->str() == "goto")
                {
                    break;
                }
            }
            if (!end)
                continue;

            const std::string name(tok->str());

            tok->deleteThis();
            tok->deleteThis();
            if (Token::Match(tok, "; %any%"))
                tok->deleteThis();

            // This label is at the end of the function.. replace all matching goto statements..
            for (std::list<Token *>::iterator it = gotos.begin(); it != gotos.end(); ++it)
            {
                Token *token = *it;
                if (token->next()->str() == name)
                {
                    // Delete the "goto name;"
                    token = token->previous();
                    token->deleteNext();
                    token->deleteNext();
                    token->deleteNext();

                    // Insert the statements..
                    bool ret = false;   // is there return
                    bool ret2 = false;  // is there return in indentlevel 0
                    std::list<Token*> links;
                    std::list<Token*> links2;
                    std::list<Token*> links3;
                    int lev = 0;
                    for (const Token *tok2 = tok; tok2; tok2 = tok2->next())
                    {
                        if (tok2->str() == "}")
                        {
                            --lev;
                            if (lev < 0)
                                break;
                        }
                        if (tok2->str() == "{")
                        {
                            ++lev;
                        }
                        else if (tok2->str() == "return")
                        {
                            ret = true;
                            if (indentlevel == 1 && lev == 0)
                                ret2 = true;
                        }
                        token->insertToken(tok2->str().c_str());
                        token = token->next();
                        token->linenr(tok2->linenr());
                        token->varId(tok2->varId());
                        if (ret2 && tok2->str() == ";")
                        {
                            break;
                        }
                        if (token->str() == "(")
                        {
                            links.push_back(token);
                        }
                        else if (token->str() == ")")
                        {
                            if (links.empty())
                            {
                                // This should never happen at this point
                                syntaxError(token, ')');
                                return;
                            }

                            Token::createMutualLinks(links.back(), token);
                            links.pop_back();
                        }
                        else if (token->str() == "{")
                        {
                            links2.push_back(token);
                        }
                        else if (token->str() == "}")
                        {
                            if (links2.empty())
                            {
                                // This should never happen at this point
                                syntaxError(token, '}');
                                return;
                            }

                            Token::createMutualLinks(links2.back(), token);
                            links2.pop_back();
                        }
                        else if (token->str() == "[")
                        {
                            links3.push_back(token);
                        }
                        else if (token->str() == "]")
                        {
                            if (links3.empty())
                            {
                                // This should never happen at this point
                                syntaxError(token, ']');
                                return;
                            }

                            Token::createMutualLinks(links3.back(), token);
                            links3.pop_back();
                        }
                    }

                    if (!ret)
                    {
                        token->insertToken("return");
                        token = token->next();
                        token->insertToken(";");
                        token = token->next();
                    }
                }
            }

            gotos.clear();
            tok = beginfunction;
            indentlevel = 0;
            continue;
        }
    }
}

void Tokenizer::simplifyNestedStrcat()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (! Token::Match(tok, "[;{}] strcat ( strcat ("))
        {
            continue;
        }

        // find inner strcat call
        Token *tok2 = tok->tokAt(3);
        while (Token::simpleMatch(tok2, "strcat ( strcat"))
        {
            tok2 = tok2->tokAt(2);
        }

        // If we have this code:
        //   strcat(strcat(dst, foo), bar);
        // We move this part of code before all strcat() calls: strcat(dst, foo)
        // And place "dst" token where the code was.
        Token *prevTok = tok2->previous();

        // Move tokens to new place
        Token::move(tok2, tok2->next()->link(), tok);
        tok = tok2->next()->link();

        // Insert the "dst" token
        prevTok->insertToken(tok2->strAt(2));

        // Insert semicolon after the moved strcat()
        tok->insertToken(";");
    }

}

void Tokenizer::duplicateEnumError(const Token * tok1, const Token * tok2, const std::string & type)
{
    if (!(_settings && _settings->_checkCodingStyle))
        return;

    std::list<ErrorLogger::ErrorMessage::FileLocation> locationList;
    ErrorLogger::ErrorMessage::FileLocation loc;
    loc.line = tok1->linenr();
    loc.file = file(tok1);
    locationList.push_back(loc);
    loc.line = tok2->linenr();
    loc.file = file(tok2);
    locationList.push_back(loc);

    const ErrorLogger::ErrorMessage errmsg(locationList,
                                           "style",
                                           std::string(type + " '" + tok2->str() +
                                                   "' hides enumerator with same name"),
                                           "variableHidingEnum");

    if (_errorLogger)
        _errorLogger->reportErr(errmsg);
    else
        Check::reportError(errmsg);
}

// Check if this statement is a duplicate definition.  A duplicate
// definition will hide the enumerator within it's scope so just
// skip the entire scope of the duplicate.
bool Tokenizer::duplicateDefinition(Token ** tokPtr, const Token * name)
{
    // check for an end of definition
    const Token * tok = *tokPtr;
    if (tok && tok->next() && Token::Match(tok->next(), ";|,|[|=|)|>"))
    {
        const Token * end = tok->next();

        if (end->str() == "[")
        {
            end = end->link()->next();
        }
        else if (end->str() == ",")
        {
            // check for function argument
            if (Token::Match(tok->previous(), "(|,"))
                return false;

            // find end of definition
            int level = 0;
            while (end && end->next() && (!Token::Match(end->next(), ";|)|>") ||
                                          (end->next()->str() == ")" && level == 0)))
            {
                if (end->next()->str() == "(")
                    level++;
                else if (end->next()->str() == ")")
                    level--;

                end = end->next();
            }
        }
        else if (end->str() == ")")
        {
            // check of function argument
            if (tok->previous()->str() == ",")
                return false;
        }

        if (end)
        {
            if (Token::Match(end, ") {")) // function parameter ?
            {
                // look backwards
                if (tok->previous()->str() == "enum" ||
                    (Token::Match(tok->previous(), "%type%") &&
                     tok->previous()->str() != "return"))
                {
                    duplicateEnumError(*tokPtr, name, "Function parameter");
                    // duplicate definition so skip entire function
                    *tokPtr = end->next()->link();
                    return true;
                }
            }
            else if (end->str() == ">") // template parameter ?
            {
                // look backwards
                if (tok->previous()->str() == "enum" ||
                    (Token::Match(tok->previous(), "%type%") &&
                     tok->previous()->str() != "return"))
                {
                    // duplicate definition so skip entire template
                    while (end && end->str() != "{")
                        end = end->next();
                    if (end)
                    {
                        duplicateEnumError(*tokPtr, name, "Template parameter");
                        *tokPtr = end->link();
                        return true;
                    }
                }
            }
            else
            {
                // look backwards
                if (Token::Match(tok->previous(), "enum|,") ||
                    (Token::Match(tok->previous(), "%type%") &&
                     tok->previous()->str() != "return"))
                {
                    duplicateEnumError(*tokPtr, name, "Variable");
                    return true;
                }
            }
        }
    }
    return false;
}

void Tokenizer::simplifyEnum()
{
    std::string className;
    int classLevel = 0;
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "class|struct|namespace %any%"))
        {
            className = tok->next()->str();
            classLevel = 0;
            continue;
        }
        else if (tok->str() == "}")
        {
            --classLevel;
            if (classLevel < 0)
                className = "";

            continue;
        }
        else if (tok->str() == "{")
        {
            ++classLevel;
            continue;
        }
        else if (Token::Match(tok, "enum {") ||
                 Token::Match(tok, "enum %type% {"))
        {
            Token * tok1;
            Token * start = tok;
            Token * end;
            Token * enumType = 0;

            if (tok->tokAt(1)->str() == "{")
                tok1 = tok->tokAt(2);
            else
            {
                enumType = tok->tokAt(1);
                tok1 = tok->tokAt(3);
            }

            end = tok1->tokAt(-1)->link();

            long lastValue = -1;
            Token * lastEnumValueStart = 0;
            Token * lastEnumValueEnd = 0;

            // iterate over all enumerators between { and }
            // Give each enumerator the const value specified or if not specified, 1 + the
            // previous value or 0 if it is the first one.
            for (; tok1 && tok1 != end; tok1 = tok1->next())
            {
                Token * enumName = 0;
                Token * enumValue = 0;
                Token * enumValueStart = 0;
                Token * enumValueEnd = 0;

                if (tok1->str() == "(")
                {
                    tok1 = tok1->link();
                    continue;
                }

                if (Token::Match(tok1->previous(), ",|{ %type% ,|}"))
                {
                    // no value specified
                    enumName = tok1;
                    lastValue++;
                    tok1->insertToken("=");
                    tok1 = tok1->next();

                    if (lastEnumValueStart && lastEnumValueEnd)
                    {
                        // previous value was an expression
                        Token * valueStart = tok1;
                        std::stack<Token *> links;
                        for (Token *tok2 = lastEnumValueStart; tok2 != lastEnumValueEnd->next(); tok2 = tok2->next())
                        {
                            tok1->insertToken(tok2->str());
                            tok1 = tok1->next();

                            // Check for links and fix them up
                            if (tok1->str() == "(" || tok1->str() == "[" || tok1->str() == "{")
                                links.push(tok1);
                            else if (tok1->str() == ")" || tok1->str() == "]" || tok1->str() == "}")
                            {
                                Token * link = links.top();

                                tok1->link(link);
                                link->link(tok1);

                                links.pop();
                            }
                        }

                        // value is previous expression + 1
                        tok1->insertToken("+");
                        tok1 = tok1->next();
                        tok1->insertToken(MathLib::toString<long>(lastValue));
                        enumValue = 0;
                        enumValueStart = valueStart->next();
                        enumValueEnd = tok1->next();
                    }
                    else
                    {
                        // value is previous numeric value + 1
                        tok1->insertToken(MathLib::toString<long>(lastValue));
                        enumValue = tok1->next();
                    }
                }
                else if (Token::Match(tok1->previous(), ",|{ %type% = %num% ,|}"))
                {
                    // value is specified numeric value
                    enumName = tok1;
                    lastValue = MathLib::toLongNumber(tok1->strAt(2));
                    enumValue = tok1->tokAt(2);
                    lastEnumValueStart = 0;
                    lastEnumValueEnd = 0;
                }
                else if (Token::Match(tok1->previous(), ",|{ %type% = "))
                {
                    // value is specified expression
                    enumName = tok1;
                    lastValue = 0;
                    tok1 = tok1->tokAt(2);
                    enumValueStart = tok1;
                    enumValueEnd = tok1;
                    int level = 0;
                    if (enumValueEnd->str() == "(" ||
                        enumValueEnd->str() == "[" ||
                        enumValueEnd->str() == "{")
                        level++;
                    while (enumValueEnd->next() &&
                           (!Token::Match(enumValueEnd->next(), "}|,") || level))
                    {
                        if (enumValueEnd->next()->str() == "(" ||
                            enumValueEnd->next()->str() == "[" ||
                            enumValueEnd->next()->str() == "{")
                            level++;
                        else if (enumValueEnd->next()->str() == ")" ||
                                 enumValueEnd->next()->str() == "]" ||
                                 enumValueEnd->next()->str() == "}")
                            level--;

                        enumValueEnd = enumValueEnd->next();
                    }
                    // remember this expression in case it needs to be incremented
                    lastEnumValueStart = enumValueStart;
                    lastEnumValueEnd = enumValueEnd;
                    // skip over expression
                    tok1 = enumValueEnd;
                }

                // find all uses of this enumerator and substitute it's value for it's name
                if (enumName && (enumValue || (enumValueStart && enumValueEnd)))
                {
                    const std::string pattern(className.empty() ? "" : (className + " :: " + enumName->str()).c_str());
                    int level = 1;
                    bool inScope = true;

                    bool exitThisScope = false;
                    int exitScope = 0;
                    bool simplify = false;
                    bool hasClass = false;
                    for (Token *tok2 = tok1->next(); tok2; tok2 = tok2->next())
                    {
                        if (tok2->str() == "}")
                        {
                            --level;
                            if (level < 0)
                                inScope = false;

                            if (exitThisScope)
                            {
                                if (level < exitScope)
                                    exitThisScope = false;
                            }
                        }
                        else if (tok2->str() == "{")
                        {
                            ++level;
                        }
                        else if (!pattern.empty() && Token::Match(tok2, pattern.c_str()))
                        {
                            simplify = true;
                            hasClass = true;
                        }
                        else if (inScope && !exitThisScope && tok2->str() == enumName->str())
                        {
                            if (Token::simpleMatch(tok2->previous(), "::") ||
                                (tok2->next() && Token::simpleMatch(tok2->next(), "::")))
                            {
                                // Don't replace this enum if it's preceded or followed by "::"
                            }
                            else if (!duplicateDefinition(&tok2, enumName))
                            {
                                simplify = true;
                                hasClass = false;
                            }
                            else
                            {
                                // something with the same name.
                                exitScope = level;
                            }
                        }

                        if (simplify)
                        {
                            if (enumValue)
                                tok2->str(enumValue->strAt(0));
                            else
                            {
                                std::stack<Token *> links;
                                tok2->str(enumValueStart->strAt(0));
                                if (tok2->str() == "(" || tok2->str() == "[" || tok2->str() == "{")
                                    links.push(tok2);
                                Token * nextToken = enumValueStart->next();
                                for (; nextToken != enumValueEnd->next(); nextToken = nextToken->next())
                                {
                                    tok2->insertToken(nextToken->str());
                                    tok2 = tok2->next();

                                    // Check for links and fix them up
                                    if (tok2->str() == "(" || tok2->str() == "[" || tok2->str() == "{")
                                        links.push(tok2);
                                    else if (tok2->str() == ")" || tok2->str() == "]" || tok2->str() == "}")
                                    {
                                        Token * link = links.top();

                                        tok2->link(link);
                                        link->link(tok2);

                                        links.pop();
                                    }
                                }
                            }

                            if (hasClass)
                            {
                                tok2->deleteNext();
                                tok2->deleteNext();
                            }

                            simplify = false;
                        }
                    }
                }
            }

            // check for a variable definition: enum {} x;
            if (end->next()->str() != ";")
            {
                Token * tempTok = end;

                tempTok->insertToken(";");
                tempTok = tempTok->next();
                tempTok->insertToken("int");
            }

            if (enumType)
            {
                const std::string pattern(className.empty() ? "" : (className + " :: " + enumType->str()).c_str());
                int level = 0;
                bool inScope = true;

                bool exitThisScope = false;
                int exitScope = 0;
                bool simplify = false;
                bool hasClass = false;
                for (Token *tok2 = end->next(); tok2; tok2 = tok2->next())
                {
                    if (tok2->str() == "}")
                    {
                        --level;
                        if (level < 0)
                            inScope = false;

                        if (exitThisScope)
                        {
                            if (level < exitScope)
                                exitThisScope = false;
                        }
                    }
                    else if (tok2->str() == "{")
                        ++level;
                    else if (!pattern.empty() && ((Token::Match(tok2, "enum") && Token::Match(tok2->next(), pattern.c_str())) || Token::Match(tok2, pattern.c_str())))
                    {
                        simplify = true;
                        hasClass = true;
                    }
                    else if (inScope && !exitThisScope && (tok2->str() == enumType->str() || (tok2->str() == "enum" && tok2->next()->str() == enumType->str())))
                    {
                        if (Token::simpleMatch(tok2->previous(), "::"))
                        {
                            // Don't replace this enum if it's preceded by "::"
                        }
                        else if (tok2->next() && tok2->next()->isName())
                        {
                            simplify = true;
                            hasClass = false;
                        }
                    }

                    if (simplify)
                    {
                        if (tok2->str() == "enum")
                            tok2->deleteNext();
                        tok2->str("int");

                        if (hasClass)
                        {
                            tok2->deleteNext();
                            tok2->deleteNext();
                        }

                        simplify = false;
                    }
                }
            }

            tok1 = start;
            while (tok1->next() && tok1->next() != end)
                tok1->deleteNext();
            tok1->deleteNext();
            if (start != _tokens)
            {
                tok1 = start->previous();
                tok1->deleteNext();
                tok = tok1;
            }
            else
                _tokens->deleteThis();
        }
    }
}


void Tokenizer::simplifyStd()
{
    std::set<std::string> f;
    f.insert("strcat");
    f.insert("strcpy");
    f.insert("strncat");
    f.insert("strncpy");
    f.insert("free");
    f.insert("malloc");
    f.insert("strdup");

    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() != "std")
            continue;

        if (Token::Match(tok->previous(), "[(,{};] std :: %var% (") &&
            f.find(tok->strAt(2)) != f.end())
        {
            tok->deleteNext();
            tok->deleteThis();
        }
    }
}

//---------------------------------------------------------------------------
// Helper functions for handling the tokens list
//---------------------------------------------------------------------------



//---------------------------------------------------------------------------

const Token *Tokenizer::getFunctionTokenByName(const char funcname[]) const
{
    for (unsigned int i = 0; i < _functionList.size(); ++i)
    {
        if (_functionList[i]->str() == funcname)
        {
            return _functionList[i];
        }
    }
    return NULL;
}


void Tokenizer::fillFunctionList()
{
    _functionList.clear();

    for (const Token *tok = _tokens; tok; tok = tok->next())
    {
        if (tok->str() == "{")
        {
            tok = tok->link();
            if (!tok)
                break;
            continue;
        }

        if (Token::Match(tok, "%var% ("))
        {
            // Check if this is the first token of a function implementation..
            for (const Token *tok2 = tok->tokAt(2); tok2; tok2 = tok2->next())
            {
                if (tok2->str() == ";")
                {
                    tok = tok2;
                    break;
                }

                else if (tok2->str() == "{")
                {
                    break;
                }

                else if (tok2->str() == ")")
                {
                    if (Token::Match(tok2, ") const| {"))
                    {
                        _functionList.push_back(tok);
                        tok = tok2;
                    }
                    else
                    {
                        tok = tok2;
                        while (tok->next() && !Token::Match(tok->next(), "[;{]"))
                            tok = tok->next();
                    }
                    break;
                }
            }
        }
    }

    // If the _functionList functions with duplicate names, remove them
    /** @todo handle when functions with the same name */
    for (unsigned int func1 = 0; func1 < _functionList.size();)
    {
        bool hasDuplicates = false;
        for (unsigned int func2 = func1 + 1; func2 < _functionList.size();)
        {
            if (_functionList[func1]->str() == _functionList[func2]->str())
            {
                hasDuplicates = true;
                _functionList.erase(_functionList.begin() + func2);
            }
            else
            {
                ++func2;
            }
        }

        if (! hasDuplicates)
        {
            ++func1;
        }
        else
        {
            _functionList.erase(_functionList.begin() + func1);
        }
    }
}

//---------------------------------------------------------------------------

// Deallocate lists..
void Tokenizer::deallocateTokens()
{
    deleteTokens(_tokens);
    _tokens = 0;
    _tokensBack = 0;
    _files.clear();
}

void Tokenizer::deleteTokens(Token *tok)
{
    while (tok)
    {
        Token *next = tok->next();
        delete tok;
        tok = next;
    }
}

//---------------------------------------------------------------------------

const char *Tokenizer::getParameterName(const Token *ftok, int par)
{
    int _par = 1;
    for (; ftok; ftok = ftok->next())
    {
        if (ftok->str() == ",")
            ++_par;
        if (par == _par && Token::Match(ftok, "%var% [,)]"))
            return ftok->str().c_str();
    }
    return NULL;
}

//---------------------------------------------------------------------------

std::string Tokenizer::fileLine(const Token *tok) const
{
    std::ostringstream ostr;
    ostr << "[" << _files.at(tok->fileIndex()) << ":" << tok->linenr() << "]";
    return ostr.str();
}

std::string Tokenizer::file(const Token *tok) const
{
    return _files.at(tok->fileIndex());
}

//---------------------------------------------------------------------------

const Token * Tokenizer::findClassFunction(const Token *tok, const std::string &classname, const std::string &funcname, int &indentlevel, bool isStruct) const
{
    if (indentlevel < 0 || tok == NULL)
        return NULL;
    /*
    // TODO: This is currently commented out as updateClassList doesn't
    // fully work yet and call to updateClassList is currently also
    // commented out.

    //std::cout << tok->str()<<"--\n";
    if( tok == _tokens || tok->str() == "class" || tok->str() == "struct")
        tok = 0;

    std::map<std::string, ClassInfo>::const_iterator iter;
    iter = _classInfoList.find(classname);
    if( iter == _classInfoList.end() )
        return NULL;

    for( size_t i = 0; i < iter->second._memberFunctions.size(); i++ )
        if( Token::Match( iter->second._memberFunctions[i]._declaration, funcname ) )
        {
            if( tok != 0 )
            {
                if( tok == iter->second._memberFunctions[i]._implementation ||
                    tok->previous() == iter->second._memberFunctions[i]._implementation)
                    tok = 0;
                continue;
            }

            return iter->second._memberFunctions[i]._implementation;
        }

        return NULL;
    */
    const std::string classPattern(std::string(isStruct ? "struct " : "class ") + classname + " :|{");
    const std::string internalPattern(std::string("!!~ ") + funcname + " (");
    const std::string externalPattern(classname + " :: " + funcname + " (");

    for (; tok; tok = tok->next())
    {
        if (indentlevel == 0 && Token::Match(tok, classPattern.c_str()))
        {
            while (tok && tok->str() != "{")
                tok = tok->next();
            if (tok)
                tok = tok->next();
            if (! tok)
                break;
            indentlevel = 1;
        }

        if (tok->str() == "{")
        {
            // If indentlevel==0 don't go to indentlevel 1. Skip the block.
            if (indentlevel > 0)
                ++indentlevel;

            else
            {
                // skip the block
                tok = tok->link();
                if (tok == NULL)
                    return NULL;

                continue;
            }
        }

        if (tok->str() == "}")
        {
            --indentlevel;
            if (indentlevel < 0)
                return NULL;
        }

        if (indentlevel == 1)
        {
            // Member function implemented in the class declaration?
            if (Token::Match(tok->previous(), internalPattern.c_str()))
            {
                const Token *tok2 = tok;
                while (tok2 && tok2->str() != "{" && tok2->str() != ";")
                    tok2 = tok2->next();
                if (tok2 && tok2->str() == "{")
                    return tok;
            }
        }

        else if (indentlevel == 0 && Token::Match(tok, externalPattern.c_str()))
        {
            if (!Token::simpleMatch(tok->previous(), "::"))
                return tok;
        }
    }

    // Not found
    return NULL;
}
//---------------------------------------------------------------------------
// Error message for bad iterator usage..

void Tokenizer::syntaxError(const Token *tok, char c)
{
    if (_settings && _settings->_debug)
    {
        _tokens->printOut();
    }

    if (!_errorLogger && tok)
    {
        std::ostringstream err;
        err << "### Unlogged error at Tokenizer::syntaxError: Invalid number of character (" << c << ")";
        if (_settings && _settings->_debug)
        {
            throw std::runtime_error(err.str());
        }
        else
        {
            std::cerr << err.str() << std::endl;
        }
        return;
    }

    std::list<ErrorLogger::ErrorMessage::FileLocation> locationList;
    if (tok)
    {
        ErrorLogger::ErrorMessage::FileLocation loc;
        loc.line = tok->linenr();
        loc.file = file(tok);
        locationList.push_back(loc);
    }

    const ErrorLogger::ErrorMessage errmsg(locationList,
                                           "error",
                                           std::string("Invalid number of character (") +
                                           c +
                                           ") " +
                                           "when these macros are defined: '" +
                                           _configuration +
                                           "'.",
                                           "syntaxError");

    if (_errorLogger)
        _errorLogger->reportErr(errmsg);
    else
        Check::reportError(errmsg);

}

void Tokenizer::cppcheckError(const Token *tok) const
{
    if (!_errorLogger && tok)
    {
        std::ostringstream err;
        err << "### Unlogged error at Tokenizer::cppcheckError";
        if (_settings && _settings->_debug)
        {
            throw std::runtime_error(err.str());
        }
        else
        {
            std::cerr << err.str() << std::endl;
        }
        return;
    }

    std::list<ErrorLogger::ErrorMessage::FileLocation> locationList;
    if (tok)
    {
        ErrorLogger::ErrorMessage::FileLocation loc;
        loc.line = tok->linenr();
        loc.file = file(tok);
        locationList.push_back(loc);
    }

    const ErrorLogger::ErrorMessage errmsg(locationList,
                                           "error",
                                           "### Internal error in Cppcheck. Please report it.",
                                           "cppcheckError");

    if (_errorLogger)
        _errorLogger->reportErr(errmsg);
    else
        Check::reportError(errmsg);
}


void Tokenizer::simplifyMathFunctions()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "atol ( %str% )"))
        {
            if (!MathLib::isInt(tok->tokAt(2)->strValue()))
            {
                // Ignore strings which we can't convert
                continue;
            }

            if (tok->previous() &&
                Token::simpleMatch(tok->previous()->previous(), "std ::"))
            {
                // Delete "std ::"
                tok = tok->previous()->previous();
                tok->deleteNext();
                tok->deleteThis();
            }

            // Delete atol(
            tok->deleteNext();
            tok->deleteThis();

            // Convert string into a number
            tok->str(MathLib::toString(MathLib::toLongNumber(tok->strValue())));

            // Delete remaining )
            tok->deleteNext();
        }
    }
}

void Tokenizer::simplifyComma()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::simpleMatch(tok, "for (") ||
            Token::Match(tok, "=|enum {"))
        {
            tok = tok->next()->link();
            if (!tok)
                break;

            continue;
        }

        if (tok->str() == "(")
        {
            tok = tok->link();
            continue;
        }

        // Skip unhandled template specifiers..
        if (Token::Match(tok, "%var% <"))
        {
            // Todo.. use the link instead.
            unsigned int comparelevel = 0;
            for (Token *tok2 = tok; tok2; tok2 = tok2->next())
            {
                if (tok2->str() == "<")
                    ++comparelevel;
                else if (tok2->str() == ">")
                {
                    if (comparelevel <= 1)
                    {
                        tok = tok2;
                        break;
                    }
                    ++comparelevel;
                }
                else if (Token::Match(tok2, "[;{}]"))
                    break;
            }
        }

        // If token after the comma is a constant number, simplification is not required.
        if (tok->str() != "," || Token::Match(tok->next(), "%num%"))
            continue;

        // We must not accept just any keyword, e.g. accepting int
        // would cause function parameters to corrupt.
        if (Token::Match(tok->next(), "delete"))
        {
            // Handle "delete a, delete b;"
            tok->str(";");
        }

        if (tok->previous() && tok->previous()->previous())
        {
            if (Token::Match(tok->previous()->previous(), "delete") &&
                tok->next()->varId() != 0)
            {
                // Handle "delete a, b;"
                tok->str(";");
                tok->insertToken("delete");
            }
            else
            {
                for (Token *tok2 = tok->previous(); tok2; tok2 = tok2->previous())
                {
                    if (tok2->str() == "=")
                    {
                        // Handle "a = 0, b = 0;"
                        tok->str(";");
                        break;
                    }
                    else if (Token::Match(tok2, "delete %var%") ||
                             Token::Match(tok2, "delete [ ] %var%"))
                    {
                        // Handle "delete a, a = 0;"
                        tok->str(";");
                        break;
                    }
                    else if (Token::Match(tok2, "[;,{}()]"))
                    {
                        break;
                    }
                }
            }
        }

        bool inReturn = false;
        Token *startFrom = NULL;    // next tokean after "; return"
        Token *endAt = NULL;        // first ";" token after "; return"

        // find "; return" pattern before comma
        for (Token *tok2 = tok; tok2; tok2 = tok2->previous())
        {
            if (Token::Match(tok2, "[;{}]"))
            {
                break;

            }
            else if (tok2->str() == "return" && Token::Match(tok2->previous(), "[;{}]"))
            {
                inReturn = true;
                startFrom = tok2->next();
                break;
            }
        }

        // find token where return ends and also count commas
        if (inReturn)
        {
            size_t commaCounter = 0;
            size_t indentlevel = 0;

            for (Token *tok2 = startFrom; tok2; tok2 = tok2->next())
            {
                if (tok2->str() == ";")
                {
                    endAt = tok2;
                    break;

                }
                else if (tok2->str() == "(")
                {
                    ++indentlevel;

                }
                else if (tok2->str() == ")")
                {
                    --indentlevel;

                }
                else if (tok2->str() == "," && indentlevel == 0)
                {
                    ++commaCounter;
                }
            }

            if (commaCounter)
            {
                indentlevel = 0;

                // change tokens:
                // "; return a ( ) , b ( ) , c ;"
                // to
                // "; return a ( ) ; b ( ) ; c ;"
                for (Token *tok2 = startFrom; tok2 != endAt; tok2 = tok2->next())
                {
                    if (tok2->str() == "(")
                    {
                        ++indentlevel;

                    }
                    else if (tok2->str() == ")")
                    {
                        --indentlevel;

                    }
                    else if (tok2->str() == "," && indentlevel == 0)
                    {
                        tok2->str(";");
                        --commaCounter;
                        if (commaCounter == 0)
                        {
                            tok2->insertToken("return");
                        }
                    }
                }

                // delete old "return"
                startFrom->previous()->deleteThis();

                tok = endAt;
                if (!tok)
                    return;
            }
        }

    }
}


void Tokenizer::removeExceptionSpecifications(Token *tok) const
{
    while (tok)
    {
        if (tok->str() == "{")
            tok = tok->link();

        else if (tok->str() == "}")
            break;

        else if (Token::simpleMatch(tok, ") throw ("))
        {
            Token::eraseTokens(tok, tok->tokAt(2)->link());
            tok->deleteNext();
        }

        else if (Token::Match(tok, "class %type%"))
        {
            while (tok && !Token::Match(tok, "[;{]"))
                tok = tok->next();
            if (tok && tok->str() == "{")
            {
                removeExceptionSpecifications(tok->next());
                tok = tok->link();
            }
        }

        tok = tok ? tok->next() : 0;
    }
}



bool Tokenizer::validate() const
{
    std::stack<const Token *> linktok;
    const Token *lastTok = 0;
    for (const Token *tok = tokens(); tok; tok = tok->next())
    {
        lastTok = tok;
        if (Token::Match(tok, "[{([]"))
        {
            if (tok->link() == 0)
            {
                cppcheckError(tok);
                return false;
            }

            linktok.push(tok);
            continue;
        }

        else if (Token::Match(tok, "[})]]"))
        {
            if (tok->link() == 0)
            {
                cppcheckError(tok);
                return false;
            }

            if (linktok.empty() == true)
            {
                cppcheckError(tok);
                return false;
            }

            if (tok->link() != linktok.top())
            {
                cppcheckError(tok);
                return false;
            }

            if (tok != tok->link()->link())
            {
                cppcheckError(tok);
                return false;
            }

            linktok.pop();
            continue;
        }

        if (tok->link() != 0)
        {
            cppcheckError(tok);
            return false;
        }
    }

    if (!linktok.empty())
    {
        cppcheckError(linktok.top());
        return false;
    }

    // Validate that the Tokenizer::_tokensBack is updated correctly during simplifications
    if (lastTok != _tokensBack)
    {
        cppcheckError(lastTok);
        return false;
    }

    return true;
}

std::string Tokenizer::simplifyString(const std::string &source)
{
    std::string str = source;
    bool escaped = false;
    for (std::string::size_type i = 0; i + 2 < str.size(); i++)
    {
        if (!escaped)
        {
            if (str[i] == '\\')
                escaped = true;

            continue;
        }

        if (str[i] == 'x')
        {
            // Hex value
            if (str[i+1] == '0' && str[i+2] == '0')
                str.replace(i, 3, "0");
            else
            {
                // We will replace all other character as 'a'
                // If that causes problems in the future, this can
                // be improved. But for now, this should be OK.
                int n = 1;
                while (n < 2 && std::isxdigit(str[i+1+n]))
                    ++n;
                --i;
                str.replace(i, 2 + n, "a");
            }
        }
        else if (MathLib::isOctalDigit(str[i]))
        {
            if (MathLib::isOctalDigit(str[i+1]) &&
                MathLib::isOctalDigit(str[i+2]))
            {
                if (str[i+1] == '0' && str[i+2] == '0')
                    str.replace(i, 3, "0");
                else
                {
                    // We will replace all other character as 'a'
                    // If that causes problems in the future, this can
                    // be improved. But for now, this should be OK.
                    --i;
                    str.replace(i, 4, "a");
                }
            }
        }

        escaped = false;
    }

    return str;
}


void Tokenizer::simplifyStructInit()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "[;{}] struct| %type% %var% = { . %type% ="))
        {
            // Goto "." and check if the initializations have an expected format
            const Token *tok2 = tok;
            while (tok2->str() != ".")
                tok2 = tok2->next();
            while (tok2 && tok2->str() == ".")
            {
                if (Token::Match(tok2, ". %type% = %num% [,}]"))
                    tok2 = tok2->tokAt(4);
                else if (Token::Match(tok2, ". %type% = %var% [,}]"))
                    tok2 = tok2->tokAt(4);
                else if (Token::Match(tok2, ". %type% = & %var% [,}]"))
                    tok2 = tok2->tokAt(5);
                else
                    break;

                if (Token::simpleMatch(tok2, ", ."))
                    tok2 = tok2->next();
            }
            if (!Token::Match(tok2, "} ;"))
                continue;

            // Known expression format => Perform simplification
            Token *vartok = tok->tokAt(3);
            if (vartok->str() == "=")
                vartok = vartok->previous();
            vartok->next()->str(";");

            Token *tok3 = vartok->tokAt(2);
            tok3->link(0);
            while (Token::Match(tok3, "[{,] . %type% ="))
            {
                tok3->str(vartok->str());
                tok3->varId(vartok->varId());
                tok3 = tok3->tokAt(5);
                while (!Token::Match(tok3, "[,}]"))
                    tok3 = tok3->next();
                if (tok3->str() == "}")
                {
                    tok3->deleteThis();
                    break;
                }
                tok3->previous()->insertToken(";");
            }
        }
    }
}


void Tokenizer::simplifyComparisonOrder()
{
    // Use "<" comparison instead of ">"
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "[;(] %any% >|>= %any% [);]"))
        {
            if (!tok->next()->isName() && !tok->next()->isNumber())
                continue;
            const std::string op1(tok->strAt(1));
            tok->next()->str(tok->strAt(3));
            tok->tokAt(3)->str(op1);
            if (tok->tokAt(2)->str() == ">")
                tok->tokAt(2)->str("<");
            else
                tok->tokAt(2)->str("<=");
        }
        else if (Token::Match(tok, "( %num% ==|!= %var% )"))
        {
            if (!tok->next()->isName() && !tok->next()->isNumber())
                continue;
            const std::string op1(tok->strAt(1));
            tok->next()->str(tok->strAt(3));
            tok->tokAt(3)->str(op1);
        }
    }
}

void Tokenizer::simplifyConst()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::Match(tok, "[;{}(,] %type% const") &&
            tok->next()->str().find(":") == std::string::npos)
        {
            tok->tokAt(2)->str(tok->tokAt(1)->str());
            tok->tokAt(1)->str("const");
        }
    }
}

void Tokenizer::getErrorMessages()
{
    syntaxError(0, ' ');
    cppcheckError(0);
}

/** find pattern */
static bool findmatch(const Token *tok1, const Token *tok2, const char pattern[])
{
    for (const Token *tok = tok1; tok && tok != tok2; tok = tok->next())
    {
        if (Token::Match(tok, pattern))
            return true;
    }
    return false;
}

void Tokenizer::simplifyWhile0()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (!Token::simpleMatch(tok, "while ( 0 )"))
            continue;

        if (Token::simpleMatch(tok->previous(), "}"))
        {
            // find "do"
            Token *tok2 = tok->previous()->link();
            tok2 = tok2 ? tok2->previous() : 0;
            if (tok2 && tok2->str() == "do" && !findmatch(tok2, tok, "continue|break"))
            {
                // delete "do {"
                tok2->deleteThis();
                tok2->deleteThis();

                // delete "} while ( 0 )"
                tok = tok->previous();
                tok->deleteNext();  // while
                tok->deleteNext();  // (
                tok->deleteNext();  // 0
                tok->deleteNext();  // )
                tok->deleteThis();  // }

                continue;
            }
        }

        // remove "while (0) { .. }"
        if (Token::simpleMatch(tok->tokAt(4), "{"))
        {
            const Token *end = tok->tokAt(4)->link();
            if (!findmatch(tok, end, "continue|break"))
            {
                Token::eraseTokens(tok, end ? end->next() : 0);
                tok->deleteThis();  // delete "while"
            }
        }

    }
}

void Tokenizer::simplifyFuncInWhile()
{
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (!Token::Match(tok, "while ( %var% ( %var% ) ) {"))
            continue;

        Token *func = tok->tokAt(2);
        Token *var = tok->tokAt(4);
        Token *end = tok->tokAt(7)->link();
        if (!end)
            break;

        tok->str("int");
        tok->insertToken("cppcheck:r");
        tok->tokAt(1)->insertToken("=");
        tok->tokAt(2)->insertToken(func->str());
        tok->tokAt(3)->insertToken("(");
        tok->tokAt(4)->insertToken(var->str());
        tok->tokAt(5)->varId(var->varId());
        tok->tokAt(5)->insertToken(")");
        tok->tokAt(6)->insertToken(";");
        tok->tokAt(7)->insertToken("while");
        tok->tokAt(9)->insertToken("cppcheck:r");
        Token::createMutualLinks(tok->tokAt(4), tok->tokAt(6));
        end->previous()->insertToken("cppcheck:r");
        end->previous()->insertToken("=");
        Token::move(func, func->tokAt(3), end->previous());
        end->previous()->insertToken(";");

        tok = end;
    }
}

void Tokenizer::simplifyStructDecl()
{
    unsigned int count = 0;

    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        // check for named struct/union
        if (Token::Match(tok, "struct|union %type% :|{"))
        {
            Token *type = tok->next();
            Token *next = tok->tokAt(2);

            while (next->str() != "{")
                next = next->next();

            tok = next->link();

            // check for named type
            if (Token::Match(tok->next(), "*|&| %type% ,|;|["))
            {
                tok->insertToken(";");
                tok = tok->next();
                tok->insertToken(type->str().c_str());
            }

            tok = next;
        }

        // check for anonymous struct/union
        else if (Token::Match(tok, "struct|union {"))
        {
            Token *tok1 = tok;

            tok = tok->next()->link();

            // check for named type
            if (Token::Match(tok->next(), "*|&| %type% ,|;|["))
            {
                std::string name;

                name = "Anonymous" + MathLib::toString<long>(count++);

                tok1->insertToken(name.c_str());

                tok->insertToken(";");
                tok = tok->next();
                tok->insertToken(name.c_str());
            }

            // unnamed anonymous struct/union so remove it
            else if (tok->next()->str() == ";")
            {
                tok1->deleteThis();
                if (tok1->next() == tok)
                {
                    tok1->deleteThis();
                    tok = tok1;
                }
                else
                    tok1->deleteThis();
                tok->deleteThis();
                if (tok->next())
                    tok->deleteThis();
                if (!tok->next())
                    return;
            }

            tok = tok1->next();
        }
    }
}

void Tokenizer::simplifyCallingConvention()
{
    const char * pattern = "__cdecl|__stdcall|__fastcall|__thiscall|__clrcall|__syscall|__pascal|__fortran|__far|__near|WINAPI|APIENTRY|CALLBACK";
    while (Token::Match(_tokens, pattern))
    {
        _tokens->deleteThis();
    }
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        while (Token::Match(tok->next(), pattern))
        {
            tok->deleteNext();
        }
    }
}

void Tokenizer::simplifyDeclspec()
{
    while (Token::simpleMatch(_tokens, "__declspec (") && _tokens->next()->link() && _tokens->next()->link()->next())
    {
        Token::eraseTokens(_tokens, _tokens->next()->link()->next());
        _tokens->deleteThis();
    }
    for (Token *tok = _tokens; tok; tok = tok->next())
    {
        if (Token::simpleMatch(tok, "__declspec (") && tok->next()->link() && tok->next()->link()->next())
        {
            Token::eraseTokens(tok, tok->next()->link()->next());
            tok->deleteThis();
        }
    }
}

