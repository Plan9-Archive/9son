#pragma src "/usr/doc/src/json"
#pragma	lib	"json.a"

enum{
	JStksz= 32,
};

typedef enum{
	JBool,
	JNum,
	JNil,
	JObj,
	JArr,
	JStr,
} Jtype;

typedef struct{
	Jtype type;
	char *start;
	char *end;
	uint nsub;
} Jtok;

typedef struct{
	uint ntok, mtok;
	Jtok *tokens;
	uint stktop;
	uint tokstk[JStksz];
} Jparser;

void Jinit(Jparser *);
void Jterm(Jparser *);

int Jtokenise(Jparser *, char *);
char *Jtokstr(Jtok *);

int Jfind(Jparser *, uint, char *);
uint Jnext(Jparser *, uint);
