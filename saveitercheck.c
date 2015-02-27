#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mip/errdefs.h>
#include <mip/printmsg.h>
#include <mip/mipmalloc.h>
#include "saveitercheck.h"
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
static char *pchsgIterSaveString=NULL;
static int isgDefaultInterval=1;
static int isgNumIterations=0;
static int bsgFirst=TRUE;

void vInitIterSaveString(int iDefaultInterval, int iNumIterations, char *pchIterSaveString)
/* Initializes the IterSaveString package. This package has two publicly accessible functions:
	viniIterSaveString and bCheckIfSaveIteration. The latter returns true if its input argument
	should be saved according the the values initialized by vInitIterSaveString. See the comments
	before bCheckIfSaveIteration for details. 
	iDefaultInterval: default interval between saved iterations.
	iNumIterations: total number of iterations. bCheckIfSaveIteration will always return true on
			this iteration. A zero means don't save iterations.
	pchIterSaveString: string specifying iterations to be saved. Iterations can either be listed or 
		a save interval at a specific iteration can be given by specifying it with a / after an
		iteratin number. Examples:
				1 2 3 4: save iterations 1, 2, 3, 4. After this, the interval for saving will be
							iDefaultInterval
				1 3 5 7 10/5 50/10 100/50: saves iterations 1, 3, 4, 7, 10, 15, ... 50, 60, 100 150, ...
*/
{
	isgDefaultInterval = iDefaultInterval;
	isgNumIterations=iNumIterations;
	pchsgIterSaveString=pchIrlStrdup(pchIterSaveString);
	bsgFirst=TRUE;
}
	

static void FindNextChangeIter(char *pchIterSaveString,
										int *piChangeIter, int *piInterval)
{
	char *pchStart, *pchEnd;
	char ch;

	/*Skip initial whitespace*/
	pchStart = pchIterSaveString + strspn(pchIterSaveString," \t,");
	vPrintMsg(6,"SaveString=%s:\n",pchIterSaveString);
	/*now look for digits*/
	pchEnd=pchStart+strspn(pchStart,"0123456789");
	if (pchEnd == pchStart){
		vErrorHandler(ECLASS_FATAL, ETYPE_SYNTAX, "FindNextChangeIter",
			"missing Change Iteration");
	}
	/* we have a string of digits, now convert it to integer */
	ch = *pchEnd;
	*pchEnd = '\0';
	*piChangeIter=atoi(pchStart);
	*pchEnd = ch;
	pchStart = pchEnd + strspn(pchEnd, " \t,");
	if (*pchStart != '/'){
		if (*pchStart != '\0' && !isdigit(*pchStart)){
			vErrorHandler(ECLASS_FATAL, ETYPE_SYNTAX, "FindNextChangeIter",
				"unexpected error parsing iteration save string: %c",
				*pchStart);
			exit(1);
		}
		/* the user did not specify a save interval, assume we save
			only at the next change iteration */
		*piInterval = 0;
	}else{
		/*the user has specified an increment*/
		pchStart++;
		/* skip whitespace */
		pchStart = pchStart + strspn(pchStart," \t,");
		/*now look for digits*/
		pchEnd=pchStart+strspn(pchStart,"0123456789");
		/* we have a string of digits, now convert it to integer */
		ch = *pchEnd;
		*pchEnd = '\0';
		*piInterval=atoi(pchStart);
		*pchEnd = ch;
		/*skip trailing whitespace*/
		pchStart = pchEnd + strspn(pchEnd, " \t,");
	}
	strcpy (pchIterSaveString,pchStart);
}
int bCheckIfSaveIteration(int iIter)
/* returns true if the image for iteration iIter is to be saved, false if not */
/* the image is saved if it is the last iteration, or if
	the number of iterations since the iteration interval was
	last changed is an integer multiple of the save interval.
	A single save interval can be specified in the DefaultInterval
	parameter, or a string of iterations/intervals can be specified
	in the save_iterations parameter. The save iterations is a list
	of iterations to save and save intervals between the iterations.
	For example, 1 2 5 10/5
	Would save iterations 1 2 5 10, and every 5th one after that */
{
	static int iLastIterChange=0;
	static int iCrntInterval=0;
	static int iNextIterChange=0;
	static int iNextInterval=0;

	if (bsgFirst){
		iCrntInterval=isgDefaultInterval;
		iNextInterval=isgDefaultInterval;
		bsgFirst=FALSE;
		if (pchsgIterSaveString == NULL || *pchsgIterSaveString == '\0'){
			iNextIterChange = iIter-1;
		}
	}
	while (iIter >= iNextIterChange && iNextIterChange >= 0){
		iLastIterChange=iNextIterChange;
		iCrntInterval=iNextInterval;
		if (pchsgIterSaveString == NULL || *pchsgIterSaveString == '\0'){
			iNextIterChange = -1;
		}else{
			FindNextChangeIter(pchsgIterSaveString,
				&iNextIterChange, &iNextInterval);
		}
		vPrintMsg(6, "Current Save Interval=%d. On iter. %d will change to %d\n",
							iCrntInterval, iNextIterChange, iNextInterval);
	}
	if (iIter == isgNumIterations) return (TRUE);
	if (iIter == iLastIterChange) return(TRUE);
	if (iCrntInterval == 0) return (FALSE);
	return((iIter - iLastIterChange) % iCrntInterval == 0);
}

void vFreeIterSaveString()
{
	IrlFree(pchsgIterSaveString);
	pchsgIterSaveString=NULL;
}

#ifdef STANDALONE
int main(int argc, char **argv)
{
	int startiter, enditer, saveinterval, iter;
	char *savestr;

	if (argc != 5){
		fprintf(stderr,"usage: itersavestring startiter enditer default_intervalsavestring\n");
		exit(1);
	}
	startiter=atoi(argv[1]);
	enditer=atoi(argv[2]);
	saveinterval=atoi(argv[3]);
	savestr=argv[4];
	vInitIterSaveString(saveinterval, enditer, savestr);

	for(iter=startiter; iter <= enditer; ++iter){
		if (bCheckIfSaveIteration(iter))
			printf("iteration %d is saved\n",iter);
	}
}

#endif
