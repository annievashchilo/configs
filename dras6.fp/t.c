#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int main()
{
	char *cp;
	static uint16_t sol_optseq[32], req_optseq[32], ren_optseq[32], *seqptr=NULL;
	//char t[] = "sol,1,2,8.39.16,req,1,2,39,8,ren,8,1,39,16,2";
	char t[] = "req,1,2,39,8,ren,8,1,39,16,2";
	if ((cp=strtok(t,", ")) == NULL){
		printf("Syntax error opt seq: %s\n", t);
		return(1);
	}
	while (cp != NULL){
		if (*cp >= '0' && *cp <= '9'){
			if (seqptr == NULL){
				printf("Need to define packet type in option sequence\n");
				return(-1);
			}
			*(seqptr++) = atoi(cp);
			
		}
		else {
			if (strncasecmp(cp,"sol",3) == 0)
				seqptr=sol_optseq;
			else if (strncasecmp(cp,"req",3) == 0)
				seqptr=req_optseq;
			else if (strncasecmp(cp,"ren",3) == 0)
				seqptr=ren_optseq;
			else {
				printf("Bad token in DHCP option sequence: %s\n", cp);
				return(-1);
			}
			while (*seqptr)
				seqptr++;
		}
		cp=strtok(NULL,", ");
		
	}
	printf("\nSolicit Options: ");
	for (seqptr=sol_optseq; *seqptr; seqptr++)
		printf("%u ", *seqptr);
	printf("\nRequest Options: ");
	for (seqptr=req_optseq; *seqptr; seqptr++)
		printf("%u ", *seqptr);
	printf("\nRenew Options: ");
	for (seqptr=ren_optseq; *seqptr; seqptr++)
		printf("%u ", *seqptr);
	printf("\n");
			
	return(0);
}
