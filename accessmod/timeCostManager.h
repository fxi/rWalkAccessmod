#include <stdio.h>
#include <math.h>
#include <gsl/gsl_poly.h>
/* Accesmod custom functions*/

/*------------------------------------------------------------------
 *               bicycle function                   
 *                                                   
 * see http://www.kreuzotter.de/english/espeed.htm  
 * constant to  put in an external file after developementi
 *-----------------------------------------------------------------*/

/* Define constant and variables */
#define  W 0 /* wind speed */
#define  Hnn 350 /* Elevation above sea */
#define  Tc 20  /* external temperature influence air density */
#define  mbike 12 /* mass of bike */
#define  mrider 80 /* mass of rider */
#define  CdA 0.4 /*Effective Drag Area */
#define  Cr 0.008 /* Rolling resistance coefficient (between 0.0025 and 0.008) */
#define  CrV 0.2 /* approximated to 0.1 */
#define  Cm 1.09 /* between 1.03 and 1.09 */
#define  rho0 1.225 /* air density at sea level, 0°C, kg/m3 */
#define  p0 101325 /* air pressur at sea level, 0°C,Pa */
#define  g 9.81 /* m/s2 */
#define  e 2.71828



/* flat values (used in formula to get initial power for given speed).*/
float betaFlat=0;
float CrVnFlat=CrV;
float CrVFlat=CrV;



/* Actual function to get final speed. Speed (expected speed on flat) and slope*/
double speedBicycle(float speed, float slope)
{ 
  double a,b,c,x0,x1,x2 ;/* var in cubic eq..*/
  int roots; /*ouput roots*/
  double speedFinal; /*final speed after slope and phys. vars. */
  float power,P ;/* Power in W */
  float Frg; /*rol friction*/
  /* rolling friction on a flat surface */
  float FrgFlat=g*(mbike+mrider)*(Cr*cos(betaFlat)+sin(betaFlat));
  speed = speed/3.6; /* value in meter and second*/
  /* Temperature in deg. kelvin ! */
  float T=Tc+273.15; 

  /* air density via barometric formula  */
  float rho=rho0*(373/T)*pow(e,(-rho0*g*(Hnn/p0)));
  /* power needed for a given base speed*/
  P = Cm*speed*(CdA*(rho/2)*pow(speed+W,2)+FrgFlat+speed+CrVnFlat);
  /*slope in radian*/  
  float beta=atan(slope); 
  /**/
  float CrVn=CrV*cos(beta);
  /* rolling friction */
  Frg=g*(mbike+mrider)*(Cr*cos(beta)+sin(beta));
  /* spped cubic function */
  a=(W+(CrVn/(CdA*rho)));
  b=pow(W,2)+((2*Frg)/(CdA*rho));
  c=-((2*P)/(Cm*CdA*rho));
  /* solve equation with gsl*/
  roots = gsl_poly_solve_cubic(a,b,c,&x0,&x1,&x2);
  /* get the absolute speed in km/h. Why does the function return a negative value for negative slope ??  */
  /*speedFinal=x0*3.6;*/
  speedFinal=fabs(x0*3.6);
  return speedFinal;
}

/*------------------------------------------------------------------
 *               hiking function
 *
 * Tobler hiking function:
 * tobler is based on a speed of 6 km/h. 
 * here, we want to set another base speed: speed.
 * So we extract coefficient to allow a modified version of tobler's formula.
 * see http://www.kreuzotter.de/english/espeed.htm  
 * constant to  put in an external file after developementi 
 *-----------------------------------------------------------------*/
double speedWalk(double speed, double slope)
{
  /* use fabs instead of abs...*/
  double topSpeed,speedFinal,testVal;
  topSpeed = speed/exp(-0.175); /* -0.175 = -3.5*0.05 */
  speedFinal = exp(-3.5*fabs(slope+0.05))*topSpeed;
  return speedFinal;
};

/*-------------------------------------------------------------------             
 *              Motorized vehicle  function 
 *
 * doesnt take in account slope. Expected to applied on road cells !
 * but.. in case of very montaneous roads, with a heavy truck:
 * The bicycle function could apply with other constants of total weight, drag area, resistance,...
 *   
 *-----------------------------------------------------------------*/
double speedMotor(double speed, double slope)
{
  double speedFinal = speed;
  return speedFinal;
};




/*-------------------------------------------------------------------             
 *              modSwitcher
 *  switch through mod of one cell to determine
 * the appropriate function to use for final speed.
 *
 *-----------------------------------------------------------------*/
double modSwitcher(int mod, double speed, double slope)
{
  double v;
  switch(mod)
  {
    case 1:
      v=speedWalk(speed,slope);
      break;
    case 2:
      v=speedBicycle(speed,slope);
      break;
    case 3:
      v=speedMotor(speed,slope);
      break;
    default: 
      v=0;
  };
  return v;
};

/*-------------------------------------------------------------------             
 *              costManager
 *          -- main function --
 * Check for the group of cells (2 if knightmove is false, 4 otherwise) to
 * extract the mode of transportation and speed from the speed map 
 * ------------- Value of mode 
 * speed map is encoded by step of thousand
 * 1 = walking 
 * 2 = bicycle
 * 3 = motorized 
 * ------------- Additional informations
 * modSpeedAdj1-3 = extracted from speed map. E.g. 2004 -> 2 is for bicycle, 4 the speed in kmh
 * slope = slope in % (r.walk :: check_dtm)
 * dist = distance between cells (r.walk :: E,W,S,N_fac or Diag_fac or V_DIAG_fac ) 
 * total_reviewed = if knight's move, 16, else 8. (r.walk total_reviewed)
 *-----------------------------------------------------------------*/
double costManager(int modSpeed,int modSpeedAdj1,int modSpeedAdj2,int modSpeedAdj3, double slope, double dist, int total_reviewed,int returnPath)
{

  /* if return path == true, slope is negative*/

  if(returnPath==1) slope = -slope;
  /* current cell values */
  int mod       = floor(modSpeed/1000);
  int speed     = round(modSpeed-mod*1000);
  /* adjacent cell mod of transportations*/
  int modAdj1   = floor(modSpeedAdj1/1000);
  int modAdj2   = floor(modSpeedAdj2/1000);
  int modAdj3   = floor(modSpeedAdj3/1000);
  /* adjacent cell speed in km/h */
  int speedAdj1 = round(modSpeedAdj1-modAdj1*1000);
  int speedAdj2 = round(modSpeedAdj2-modAdj2*1000);
  int speedAdj3 = round(modSpeedAdj3-modAdj3*1000);
 
  /* output var */
  double speedCurrent;
  double speedFinal;
  double costTimeFinal;
  int maxVal;

  /*maxVal is the maximum time allowed for crossing a cell : 18.2 hour 
   * it's set to avoid to much depth in final map, e.g. in case of dividing by a speed of zero..*/

  maxVal=65535; /* max value in 16 bit map 2^16-1*/

  /* get speed for the present cell according to its mode,speed and slope*/
  speedCurrent = modSwitcher(mod,speed,slope);
  /*case of knight's move (3 adjacent cells) there is 4 cell to compute.
   * TODO check for equality between cells mode AND speed to avoid recalculation 
   * at every step. Caution : in the worst case, all 4 cells have different speed
   * AND mode of transportation. A lot of possibilities. */
  if(total_reviewed==16){
    speedFinal=(
        speedCurrent+
        modSwitcher(modAdj1,speedAdj1,slope)+
        modSwitcher(modAdj2,speedAdj2,slope)+
        modSwitcher(modAdj3,speedAdj3,slope)
        )/4; 
  }else{
    speedFinal=(speedCurrent+modSwitcher(modAdj1,speedAdj1,slope))/2; 
  };

  /* Return cost (s) for the provided distance*/
  /* To be checked : round value to have total travel cost in integer of a second */
  costTimeFinal=round((1/(speedFinal/3.6))*dist);
if(costTimeFinal>maxVal){costTimeFinal=maxVal;};/* max value allowed by 16 bit map (2^16-1)*/
  return costTimeFinal;
}

