
#define IMAGE_EXTENSION ".im"

// Setup.c
char *pchGetNormBase(char *pchBase);
void vGetParms(IrlParms_t *psParms, Options_t *psOptions, int iMode);
int iParseModelString(char *pchModelStr, char *pchName);
void vGetEffectsToModel(int *pbModelAtn, int *pbModelDrf, int *pbModelSrf);
PrjView_t *psSetupPrjViews(IrlParms_t *psParms);
char *pchGetSrfKrnlFname(void);
char *pchGetDrfTabFname(void);
int iSetupFromCmdLine(int iArgc, char **ppchArgv, IrlParms_t *psIrlParms, Options_t *psOptions, PrjView_t **ppsPrjViews, char **ppchSrfKrnlFile, char **ppchDrfTabFile, char **ppchLogFile, char **ppchMsgFile, char **ppchOutBase, float **ppfPrjImage, float **ppfAtnMap, float **ppfReconImage, float **ppfScatterEstimate, float *pfPrimaryFac, int iMode, char *(pUsageMsg(void)));
void *pvLocalMalloc(int iSize, char *pchName);

// GetImages.c
void vGetImageSizes(char *pchPrjImageName, IrlParms_t *psParms, int iMode);
float *pfGetPrjImage(char *pchPrjImageName, IrlParms_t *psParms, PrjView_t *psViews);
float *pfGetAtnMap(char *pchAtnMapName, IrlParms_t *psParms);
float *pfGetScatterEstimate(IrlParms_t *psParms, PrjView_t *psViews);
float *pfGetInitialEst(char *pchInitImageName, IrlParms_t *psParms);
float *pfGetActImage(char *pchActImageName, IrlParms_t *psParms);
float *pfGetOrbitImage(char *pchFname, IrlParms_t *psParms);

// MeasToModPrj.c
void vMeasToModPrj(int nBins, int nRotPixs, int nSlices, float Left, float BinWidth, float PixelWidth, float *RawPrjData, float *ModPrjData);
void Interp_bck( float *InPrjData, float *OutPrjData, IrlParms_t *psParms, PrjView_t psView, float *Sum);

