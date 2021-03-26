#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

unsigned allocated;

static void *YAMLMalloc(size_t bytes)
{
    allocated++;
    return malloc(bytes);
}

static void YAMLFree(void *block)
{
    allocated--;
    free(block);
}

typedef struct
{
    const char *Name;
    char *Source;
    char *Pos;
} YAML;

typedef enum
{
    YAMLVal_String,
    YAMLVal_Number,
} YAMLValueKind;

typedef struct YAMLNode YAMLNode;
struct YAMLNode
{
    YAMLValueKind Kind;
    const char *Key;
    int KeyLen;
    union
    {
        struct
        {
            const char *StrVal;
            int StrValLen;
        };
        double NumVal;
    };
    
    YAMLNode *Next;
};

YAML *OpenYAML(const char *path)
{
    FILE *fs = fopen(path, "rb");
    fseek(fs, 0, SEEK_END);
    long len = ftell(fs);
    char *src = YAMLMalloc(len + 1);
    rewind(fs);
    fread(src, 1, len, fs);
    fclose(fs);
    src[len] = 0;
    
    YAML *yaml = YAMLMalloc(sizeof(YAML));
    yaml->Name = path;
    yaml->Source = src;
    return yaml;
}

static _Bool YAML__IsWhiteSpace(char c)
{ return c == '\n' || c == '\t' || c == ' ' || c == '\r'; }

static _Bool YAML__IsIdentifierBegin(char c)
{ return c >= 'A' && c <= 'Z' || c >= 'a' && c <= 'z' || c == '$' || c == '_'; }

static _Bool YAML__IsNumber(char c)
{ return c >= '0' && c <= '9' || c == '$' || c == '.' || c == '-'; }

static int YAML__IsQuote(char c)
{ return ((c == '\'') << 1) + (c == '\"'); }

static _Bool YAML__IsIdentifier(char c)
{ return YAML__IsIdentifierBegin(c) || YAML__IsNumber(c); }

#define NewStrYAMLNode(node, key, keyLen, strVal, strLen) YAMLNode *node = YAMLMalloc(sizeof(YAMLNode)); \
    node->Kind = YAMLVal_String; node->Key = key; node->KeyLen = keyLen; node->StrVal = strVal; node->StrValLen = strLen; node->Next = NULL;
#define NewNumYAMLNode(node, key, keyLen, numVal) YAMLNode *node = YAMLMalloc(sizeof(YAMLNode)); \
    node->Kind = YAMLVal_Number; node->Key = key; node->KeyLen = keyLen; node->NumVal = numVal; node->Next = NULL;

#define Current *this->Pos
#define WHITESPACE() while (YAML__IsWhiteSpace(Current)) this->Pos++
static YAMLNode *ParseYAML__(YAML *this)
{
    YAMLNode *node;
    assert(YAML__IsIdentifierBegin(Current));

    // Parse Key:
    const char *keyBeg = this->Pos++;
    while (YAML__IsIdentifier(Current))
        this->Pos++;
    int keyLen = this->Pos - keyBeg;

    // Colon :
    WHITESPACE();
    assert(*this->Pos++ == ':');

    // Parse Value (String)
    WHITESPACE();
    int quoteTy = YAML__IsQuote(Current);
    if (quoteTy)
    {
        this->Pos++;
        const char *valBeg = this->Pos++;
        while (YAML__IsQuote(Current) != quoteTy)
            this->Pos++;
        int valLen = this->Pos++ - valBeg;
        NewStrYAMLNode(node, keyBeg, keyLen, valBeg, valLen);
        return node;
    }
    else if (YAML__IsNumber(Current))
    {
        const char *valBeg = this->Pos++;
        while (YAML__IsNumber(Current))
            this->Pos++;
        double val = atof(valBeg);
        NewNumYAMLNode(node, keyBeg, keyLen, val);
        return node;
    }
    else
    {
        const char *valBeg = this->Pos++;
        while (Current != '\n' && Current != '\r' && Current != '\0')
            this->Pos++;
        int valLen = this->Pos - valBeg;
        NewStrYAMLNode(node, keyBeg, keyLen, valBeg, valLen);
        return node;
    }
}
#undef NewStrYAMLNode

void PrintYAML(YAMLNode *this);
YAMLNode *ParseYAML(YAML *this)
{
    this->Pos = this->Source;

    YAMLNode base = (YAMLNode) { 0 };
    YAMLNode *node = ParseYAML__(this);
    base.Next = node;
    WHITESPACE();
    while (Current)
    {
        YAMLNode *parsedNode = ParseYAML__(this);
        // PrintYAML(parsedNode);
        node->Next = parsedNode;
        while (node->Next)
            node = node->Next;
        WHITESPACE();
    }

    return base.Next;
}
#undef WHITESPACE

void DeleteYAML(YAML *this)
{
    if (this)
    {
        YAMLFree(this->Source);
        YAMLFree(this);
    }
}

void DeleteYAMLNodes(YAMLNode *this)
{
    if (this)
        DeleteYAMLNodes(this->Next);
    YAMLFree(this);
}

void DeleteYAMLNode(YAMLNode *this)
{
    YAMLFree(this);
}
#undef Current

void PrintYAML(YAMLNode *this)
{
    if (this)
    {
        switch (this->Kind)
        {
        case YAMLVal_String:
            printf("YAML Node { Key: '%.*s', Val: '%.*s' }\n", this->KeyLen, this->Key, this->StrValLen, this->StrVal);
            break;
        case YAMLVal_Number:
            printf("YAML Node { Key: '%.*s', Val: %f }\n", this->KeyLen, this->Key, this->NumVal);
            break;
        }
        PrintYAML(this->Next);
    }
}

// Benchmark function (ms)
#include <time.h>
static double GetTime()
{
    double time = (double)clock() / CLOCKS_PER_SEC;
    return time * 1000;
}

double MyYAMLTest(_Bool print)
{
    double begin = GetTime();
    YAML *yaml = OpenYAML("Yaml.yml");
    YAMLNode *nodes = ParseYAML(yaml);
    double end = GetTime();
    if (print)
        PrintYAML(nodes);
    DeleteYAMLNodes(nodes);
    DeleteYAML(yaml);

    return end - begin;
}

int main()
{
    double totalTime, time;
    int i;
    for (i = 0; i < 1000; i++)
    {
        time = MyYAMLTest(0);
        totalTime += time;
        // printf("Took %fms\n", time); // Comment if you don't want to print
    }

    MyYAMLTest(1);
    printf("Average (%i) is %fms\n", i, totalTime / (double)i);
    printf("Memory Leaks: %i\n", allocated);
}