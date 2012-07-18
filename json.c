#include<u.h>
#include<libc.h>
#include<json.h>

static void*
erealloc(void *ptr, ulong sz)
{
	ptr = realloc(ptr, sz);
	if(ptr == nil)
		sysfatal("realloc: %r");
	return ptr;
}

void
Jinit(Jparser *p)
{
	memset(p, 0, sizeof *p);
}

void
Jterm(Jparser *p)
{
	if(p->mtok > 0 || p->tokens != nil)
		free(p->tokens);
}

static Jtok*
gettok(Jparser *p)
{
	uint i;
	
	if(p->ntok == p->mtok){
		p->mtok = (p->ntok << 1) + 1;
		p->tokens = erealloc(p->tokens, p->mtok * sizeof *p->tokens);
		for(i = p->ntok; i < p->mtok; i++)
			memset(&p->tokens[i], 0, sizeof *p->tokens);
	}
	
	return &p->tokens[p->ntok++];
}

static char *
parsestr(Jparser *p, char *s)
{
	Jtok *tok;
	
	tok = gettok(p);
	tok->type = JStr;
	for(tok->start = ++s; *s != '\0' && utfrune("\"", *s) == nil; s++)
		if(utfrune("\\", *s) != nil)
			s++;
	tok->end = s;
	return s;
}

static char *
saccept(char *src, char *str)
{
	while(*src != '\0' && utfrune(str, *src) != nil)
		src++;
	return src;
}

static void
incnsub(Jparser *p)
{
	if(p->stktop > 0)
		p->tokens[p->tokstk[p->stktop - 1]].nsub++;
}

int
Jtokenise(Jparser *p, char *s)
{
	Jtok *tok;
	Jtype type;
	
	memset(p->tokens, 0, p->mtok * sizeof *p->tokens);
	
	for(; *s != '\0'; s++){
		if(utfrune("\t\r\n ", *s) != nil)
			continue;
			
		if(utfrune(":", *s) != nil)
			if(p->ntok > 0 && p->tokens[p->ntok - 1].type == JStr)
				continue;
			else{
				werrstr("unexpected :");
				return -1;
			}
		
		if(utfrune(",", *s) != nil)
			if(p->stktop != 0)
				continue;
			else{
				werrstr(", outside of object or array");
				return -1;
			}
		
		if(utfrune("{[", *s) != nil){
			tok = gettok(p);
			tok->start = s;
			if(p->stktop == JStksz){
				werrstr("stack overflow");
				return -1;
			}
			incnsub(p);
			p->tokstk[p->stktop++] = p->ntok - 1;
			tok->type = utfrune("{", *s) != nil ? JObj:JArr;
			continue;
		}
		
		if(utfrune("}]", *s) != nil){
			type = utfrune("}", *s) != nil ? JObj:JArr;
			if(p->ntok == 0)
				goto Jmatcherr;
			for(tok = &p->tokens[p->ntok - 1]; tok >= p->tokens; tok--){
				if(tok->start != nil && tok->end == nil){
					if(tok->type != type){
						werrstr("expected %c", type == JObj ? '}':']');
						return -1;
					}
					if(p->stktop == 0){
						werrstr("stack underflow");
						return -1;
					}
					p->stktop--;
					tok->end = s + 1;
					goto Jnext;
				}
			}
Jmatcherr:
			werrstr("no matching %c", type == JObj ? '{':'[');
			return -1;
		}
		
		if(utfrune("\"", *s) != nil){
			s = parsestr(p, s);
			if(*s == '\0' || utfrune("\"", *s) == nil){
				werrstr("expected \"");
				return -1;
			}
			incnsub(p);
			continue;
		}
		
		if(utfrune("tf", *s) != nil){
			tok = gettok(p);
			tok->type = JBool;
			tok->start = s;
			s = saccept(s, "truefalse");
			tok->end = s--;
			incnsub(p);
			continue;
		}
		
		if(utfrune("n", *s) != nil){
			tok = gettok(p);
			tok->type = JNil;
			tok->start = s;
			s = saccept(s, "null");
			tok->end = s--;
			incnsub(p);
			continue;
		}
		
		/* Try and parse a number */
		tok = gettok(p);
		tok->type = JNum;
		tok->start = s;
		s = saccept(s, "+-");
		s = saccept(s, "0123456789");
		s = saccept(s, ".");
		s = saccept(s, "0123456789");
		s = saccept(s, "eE");
		s = saccept(s, "+-");
		s = saccept(s, "0123456789");
		tok->end = s;
		
		if(*s != '\0' && utfrune("\t\r\n }],", *s) == nil){
			werrstr("number format error");
			return -1;
		}
		
		incnsub(p);
		s--;
Jnext:;
	}
	
	if(p->stktop != 0){
		werrstr("unexpected end of input");
		return -1;
	}
	
	return 0;
}

char *
Jtokstr(Jtok *t)
{
	static char *buf;
	static ulong sz = 0;
	ulong len;
	char *src, *dst;
	
	len = t->end - t->start;
	if(sz <= len){
		sz = len + 1;
		buf = erealloc(buf, sz);
	}
	
	for(src = t->start, dst = buf; len > 0; len--){
		if(*src == '\\')
			src++, len--;
		*dst++ = *src++;
	}
	*dst = '\0';
	return buf;
}

uint
Jnext(Jparser *p, uint i)
{
	int j;
	if(i >= p->ntok)
		return i;
	if(p->tokens[i].type != JObj && p->tokens[i].type != JArr)
		return i + 1;
	for(j = i; j < p->ntok && p->tokens[i].end > p->tokens[j].start; j++);
	return j;
}

int
Jfind(Jparser *p, uint r, char *name)
{
	uint i, j;
	assert(r < p->ntok);
	assert(p->tokens[r].type == JObj);
	assert(p->tokens[r].nsub % 2 == 0);
	
	for(i = 0, j = r + 1; i < p->tokens[r].nsub; i += 2){
		assert(p->tokens[j].type == JStr);
		if(strcmp(Jtokstr(&p->tokens[j]), name) == 0)
			return j + 1;
		j = Jnext(p, j + 1);
	}
	
	if(j >= p->ntok)
		return -1;
	return j;
}
