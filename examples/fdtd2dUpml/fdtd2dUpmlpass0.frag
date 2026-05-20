#version 460
precision highp float;

// UBO must be aligned to 16-byte manually to match c++
layout (binding = 0) uniform Uniform {
  vec4 iResolution; // viewport resolution (in pixels)
  int iFrame[4]; // shader playback frame
  vec4 iChannelResolution[4]; // channel resolution (in pixels), only uses [0] and [1]
} u;

#define iResolution u.iResolution
#define iFrame u.iFrame[0]
#define iChannelResolution u.iChannelResolution

layout (binding = 1) uniform sampler2D iChannel0; // Buffer A; 4ch, float32, linear, repeat
layout (binding = 2) uniform sampler2D iChannel1; // Buffer B; 4ch, float32, linear, repeat
layout (binding = 3) uniform sampler2D iChannel2; // Buffer C; 4ch, float32, linear, repeat
layout (binding = 4) uniform sampler2D iChannel3; // Buffer D; 4ch, float32, linear, repeat

layout(location = 0) out vec4 outColour0;
layout(location = 1) out vec4 outColour1;

// created by pocdn - 2021
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.

// 2D Finite-Difference Time-Domain 
// ---------------
// Case with absorbing boundary conditions 

const float cc=2.99792458e8;
const float muz=4.0*3.14*1.0e-7;
const float epsz=1.0/(cc*cc*muz);
const float etaz=sqrt(muz/epsz);

const float mur=1.0;
const float epsr=1.0;
const float eta=etaz*sqrt(mur/epsr);

const float freq=5.0e+9;
const float lambda=cc/freq;
const float omega=2.0*3.14*freq;

const float delta=0.002;
const float dt=delta*sqrt(epsr*mur)/(2.0*cc);

const float rtau=50.0e-12;
const float tau=rtau/dt;
const float ndelay=3*tau;
const float J0=-1.0*epsz;

const float C1=1.0;
const float C2=dt;
const float C3=1.0;
const float C4=1.0/2.0/epsr/epsr/epsz/epsz;
const float C5=2.0*epsr*epsz;
const float C6=2.0*epsr*epsz;
 
const float D1=1.0;
const float D2=dt;
const float D3=1.0;
const float D4=1.0/2.0/epsr/epsz/mur/muz;
const float D5=2.0*epsr*epsz;
const float D6=2.0*epsr*epsz;

int upml = 10;        // Thickness of PML boundaries
int ih_tot = int(iChannelResolution[0].x);
int jh_tot = int(iChannelResolution[0].y);
int ie_tot = ih_tot - 1;
int je_tot = jh_tot - 1;
int ie = ie_tot - 2*upml;
int je = je_tot - 2*upml;
int ih = ie + 1; 
int jh = je + 1; 
int ih_bc = upml + 1;
int jh_bc = upml + 1;
int is = 2*upml; //ih_tot/2;
int js = jh_tot/2;

vec4 getH(vec2 p){
    return texture(iChannel0,p/iResolution.xy);
}

vec4 getE(vec2 p){
    return texture(iChannel1,p/iResolution.xy);
}

vec4 getB(vec2 p){
    return texture(iChannel2,p/iResolution.xy);
}

vec4 getD(vec2 p){
    return texture(iChannel3,p/iResolution.xy);
}

vec4[6] materialConstants(in int i, in int j){
   
    // the material constants default to vacuum
    float D1hx=D1, D2hx=D2, D3hx=D3, D4hx=D4, D5hx=D5, D6hx=D6;
    float D1hy=D1, D2hy=D2, D3hy=D3, D4hy=D4, D5hy=D5, D6hy=D6;
    float C1ez=C1, C2ez=C2, C3ez=C3, C4ez=C4, C5ez=C5, C6ez=C6;

    float rmax=exp(-16.);  //desired reflection error, designated as R(0) in Equation 7.62 
    
    float orderbc=4;      //order of the polynomial grading, designated as m in Equation 7.60a,b
    
    //   x-varying material properties
    float delbc=upml*delta;
    float sigmam=-log(rmax)*(orderbc+1.0)/(2.0*eta*delbc); 
    float sigfactor=sigmam/(delta*pow(delbc,orderbc)*(orderbc+1.0));
    float kmax=1;
    float kfactor=(kmax-1.0)/delta/(orderbc+1.0)/pow(delbc,orderbc);
   
//    for i=1:upml
//      for j=1:jh_tot
        
        // Coefficients for field components in the center of the grid cell
        float x1=(upml-i+1)*delta;
        float x2=(upml-i)*delta;
        if (ie_tot-upml+1<=i && i<=ie_tot-1+1) {
          x1=(upml-(ie_tot-1+2-i)+1)*delta;
          x2=(upml-(ie_tot-1+2-i))*delta;
        }
        float sigma=sigfactor*(pow(x1,orderbc+1)-pow(x2,orderbc+1));
        float ki=1+kfactor*(pow(x1,orderbc+1)-pow(x2,orderbc+1));
        float facm=(2*epsr*epsz*ki-sigma*dt);
        float facp=(2*epsr*epsz*ki+sigma*dt);
    
        if (1<=j && j<=jh_tot) {
          if (1<=i && i<=upml) {
//        D3hy(i,j)=facm/facp;
//        D4hy(i,j)=1.0/facp/mur/muz;
            D3hy=facm/facp;
            D4hy=1.0/facp/mur/muz;
          } else if (ie_tot-upml+1<=i && i<=ie_tot-1+1) {
//        D3hy(ie_tot-i+1,j)=facm/facp;
//        D4hy(ie_tot-i+1,j)=1.0/facp/mur/muz;
            D3hy=facm/facp;
            D4hy=1.0/facp/mur/muz;
          } 

          // PEC walls
          if (i==1) {
//        C1ez(1,j)=-1.0;
//        C2ez(1,j)=0.0;
            C1ez=-1.0;
            C2ez=0.0;
          } else if (i==ih_tot) { 
//        C1ez(ih_tot,j)=-1.0;
//        C2ez(ih_tot,j)=0.0;
            C1ez=-1.0;
            C2ez=0.0;
          }
        };
    
        // Coefficients for field components on the grid cell boundary
        x1=(upml-i+1.5)*delta;
        x2=(upml-i+0.5)*delta;
        if (ih_tot-upml+1<=i && i<=ih_tot-1+1) {
          x1=(upml-(ih_tot-1+2-i)+1.5)*delta;
          x2=(upml-(ih_tot-1+2-i)+0.5)*delta;
        }
        sigma=sigfactor*(pow(x1,orderbc+1)-pow(x2,orderbc+1));
        ki=1.0+kfactor*(pow(x1,orderbc+1)-pow(x2,orderbc+1));
        facm=(2.0*epsr*epsz*ki-sigma*dt);
        facp=(2.0*epsr*epsz*ki+sigma*dt);
    
        if (1<=i && i<=upml) {
          if (1<=j && j<=jh_tot) {
//        C1ez(i,j)=facm/facp;
//        C2ez(i,j)=2.0*epsr*epsz*dt/facp;
            C1ez=facm/facp;
            C2ez=2.0*epsr*epsz*dt/facp;
          } 
          if (1<=j && j<=je_tot) {
//        D5hx(i,1:je_tot)=facp;
//        D6hx(i,1:je_tot)=facm;
            D5hx=facp;
            D6hx=facm;
          }
        } else if (ih_tot-upml+1<=i && i<=ih_tot-1+1) {
          if (1<=j && j<=jh_tot) {
//        C1ez(ih_tot-i+1,j)=facm/facp;
//        C2ez(ih_tot-i+1,j)=2.0*epsr*epsz*dt/facp;
            C1ez=facm/facp;
            C2ez=2.0*epsr*epsz*dt/facp;
          } 
          if (1<=j && j<=je_tot) {
//        D5hx(ih_tot-i+1,1:je_tot)=facp;
//        D6hx(ih_tot-i+1,1:je_tot)=facm;
            D5hx=facp;
            D6hx=facm;
          }
        };

        
//    end
    
    //   y-varying material properties
    delbc=upml*delta;
    sigmam=-log(rmax)*epsr*epsz*cc*(orderbc+1.0)/(2.0*delbc); 
    sigfactor=sigmam/(delta*pow(delbc,orderbc)*(orderbc+1.0));
    kmax=1.0;
    kfactor=(kmax-1.0)/delta/(orderbc+1.0)/pow(delbc,orderbc);
  
//    for j=1:upml
        
        // Coefficients for field components in the center of the grid cell
        float y1=(upml-j+1)*delta;
        float y2=(upml-j)*delta;
        if (je_tot-upml+1<=j && j<=je_tot-1+1) {
          y1=(upml-(je_tot-1+2-j)+1)*delta;
          y2=(upml-(je_tot-1+2-j))*delta;
        }
        sigma=sigfactor*(pow(y1,orderbc+1)-pow(y2,orderbc+1));
        ki=1+kfactor*(pow(y1,orderbc+1)-pow(y2,orderbc+1));
        facm=(2*epsr*epsz*ki-sigma*dt);
        facp=(2*epsr*epsz*ki+sigma*dt);
       
        if (1<=i && i<=ih_tot) {
          if (1<=j && j<=upml) {
//        D1hx(1:ih_tot,j)=facm/facp;
//        D2hx(1:ih_tot,j)=2*epsr*epsz*dt/facp;
            D1hx=facm/facp;
            D2hx=2*epsr*epsz*dt/facp;
          } else if (je_tot-upml+1<=j && j<=je_tot-1+1) {
//        D1hx(1:ih_tot,je_tot-j+1)=facm/facp;
//        D2hx(1:ih_tot,je_tot-j+1)=2*epsr*epsz*dt/facp;
            D1hx=facm/facp;
            D2hx=2*epsr*epsz*dt/facp;
          }

          //   PEC walls
          if (j==1) {
//        C3ez(1:ih_tot,1)=-1;
//        C4ez(1:ih_tot,1)=0;
            C3ez=-1;
            C4ez=0;
          } else if (j==jh_tot) {
//        C3ez(1:ih_tot,jh_tot)=-1;
//        C4ez(1:ih_tot,jh_tot)=0;
            C3ez=-1;
            C4ez=0;
          }
        }
       
        // Coefficients for field components on the grid cell boundary
        y1=(upml-j+1.5)*delta;
        y2=(upml-j+0.5)*delta;
        if (jh_tot-upml+1<=j && j<=jh_tot-1+1) {
          y1=(upml-(jh_tot-1+2-j)+1.5)*delta;
          y2=(upml-(jh_tot-1+2-j)+0.5)*delta;
        }
        sigma=sigfactor*(pow(y1,orderbc+1)-pow(y2,orderbc+1));
        ki=1+kfactor*(pow(y1,orderbc+1)-pow(y2,orderbc+1));
        facm=(2*epsr*epsz*ki-sigma*dt);
        facp=(2*epsr*epsz*ki+sigma*dt);    
         
        if (1<=j && j<=upml) {
          if (1<=i && i<=ih_tot) {
 //       C3ez(1:ih_tot,j)=facm/facp;
 //       C4ez(1:ih_tot,j)=1/facp/epsr/epsz;
            C3ez=facm/facp;
            C4ez=1/facp/epsr/epsz;
          }
          if (1<=i && i<=ie_tot) {
 //       D5hy(1:ie_tot,j)=facp;
 //       D6hy(1:ie_tot,j)=facm;
            D5hy=facp;
            D6hy=facm;
          }
        } else if (jh_tot-upml+1<=j && j<=jh_tot-1+1) {
          if (1<=i && i<=ih_tot) {
 //       C3ez(1:ih_tot,jh_tot-j+1)=facm/facp;
 //       C4ez(1:ih_tot,jh_tot-j+1)=1/facp/epsr/epsz;   
            C3ez=facm/facp;
            C4ez=1/facp/epsr/epsz;
          }
          if (1<=i && i<=ie_tot) {
 //       D5hy(1:ie_tot,jh_tot-j+1)=facp;
 //       D6hy(1:ie_tot,jh_tot-j+1)=facm;
            D5hy=facp;
            D6hy=facm;
          }
        }
    
//    end

    //   z-varying material properties
    delbc=upml*delta;
    sigmam=-log(rmax)*epsr*epsz*cc*(orderbc+1)/(2*delbc); 
    sigfactor=sigmam/(delta*pow(delbc,orderbc)*(orderbc+1));
    kmax=1;
    kfactor=(kmax-1)/delta/(orderbc+1)/pow(delbc,orderbc);
    
//    for k=1:1
      int k = 1;
    
        // Coefficients for field components in the center of the grid cell
        float z1=(upml-k+1)*delta;
        float z2=(upml-k)*delta;
        sigma=sigfactor*(pow(z1,orderbc+1)-pow(z2,orderbc+1));
        ki=1+kfactor*(pow(z1,orderbc+1)-pow(z2,orderbc+1));
        facm=(2*epsr*epsz*ki-sigma*dt);
        facp=(2*epsr*epsz*ki+sigma*dt);
        
        if (1<=j && j<=jh_tot) {
          if (1<=i && i<=ih_tot) {
//        C5ez(1:ih_tot,1:jh_tot)=facp;
//        C6ez(1:ih_tot,1:jh_tot)=facm;
            C5ez=facp;
            C6ez=facm;
          }
          if (1<=i && i<=ie_tot) {
//        D1hy(1:ie_tot,1:jh_tot)=facm/facp;
//        D2hy(1:ie_tot,1:jh_tot)=2*epsr*epsz*dt/facp;
            D1hy=facm/facp;
            D2hy=2*epsr*epsz*dt/facp;
          }
        }
        if (1<=j && j<=je_tot) {
          if (1<=i && i<=ih_tot) {
//        D3hx(1:ih_tot,1:je_tot)=facm/facp;
//        D4hx(1:ih_tot,1:je_tot)=1/facp/mur/muz;
            D3hx=facm/facp;
            D4hx=1/facp/mur/muz;
          }
        }
        
        // Coefficients for field components on the grid cell boundary
//        z1=(upml-k+1.5)*delta;
//        z2=(upml-k+0.5)*delta;
//        sigma=sigfactor*(z1^(orderbc+1)-z2^(orderbc+1));
//        ki=1+kfactor*(z1^(orderbc+1)-z2^(orderbc+1));
//        facm=(2*epsr*epsz*ki-sigma*dt);
//        facp=(2*epsr*epsz*ki+sigma*dt);
        
//    end

    vec4[6] ret;
    ret[0] = vec4(D1hx, D1hy, C1ez, 0.);
    ret[1] = vec4(D2hx, D2hy, C2ez, 0.);
    ret[2] = vec4(D3hx, D3hy, C3ez, 0.);
    ret[3] = vec4(D4hx, D4hy, C4ez, 0.);
    ret[4] = vec4(D5hx, D5hy, C5ez, 0.);
    ret[5] = vec4(D6hx, D6hy, C6ez, 0.);
   return ret;
}

void mainImage( out vec4 hp, out vec4 bp, in vec2 fragCoord ){
    // +0.5 puts i,j on integer grid points starting at index=1,1
    int i = int(fragCoord.x+0.5);
    int j = int(fragCoord.y+0.5);

    vec4[6] material = materialConstants(i,j); 
    vec4 D1h = material[0];
    vec4 D2h = material[1];
    vec4 D3h = material[2];
    vec4 D4h = material[3];
    vec4 D5h = material[4];
    vec4 D6h = material[5];
    
    vec4 h =   getH(fragCoord);
    vec4 e =   getE(fragCoord);
    vec4 epY = getE(fragCoord+vec2( 0, 1));
    vec4 epX = getE(fragCoord+vec2( 1, 0));
    vec4 b =   getB(fragCoord);
    
    // 2:ie_tot,1:je_tot
    if (2<=i && i<=ie_tot && 1<=j && j<=je_tot) { 
      bp.x = D1h.x*b.x - D2h.x*(epY.z - e.z)/delta;
      hp.x = D3h.x*h.x + D4h.x*(D5h.x*bp.x - D6h.x*b.x);
    } else {
      hp = vec4(0.);
      bp = vec4(0.);
    }
    // (1:ie_tot,2:je_tot)
    if (1<=i && i<=ie_tot && 2<=j && j<=je_tot) { 
      bp.y = D1h.y*b.y + D2h.y*(epX.z - e.z)/delta;
      hp.y = D3h.y*h.y + D4h.y*(D5h.y*bp.y - D6h.y*b.y);
    } else {
      hp = vec4(0.);
      bp = vec4(0.);
    }
}

void main() {
    mainImage( outColour0, outColour1, (iResolution.xy/iChannelResolution[0].xy)*gl_FragCoord.xy );
}

