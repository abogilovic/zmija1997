#define _XTAL_FREQ 8000000
#include <xc.h>
#include <stdlib.h>
#include <math.h>

#pragma config FOSC=HS,WDTE=OFF,PWRTE=OFF,MCLRE=ON,CP=OFF,CPD=OFF,BOREN=OFF,CLKOUTEN=OFF
#pragma config IESO=OFF,FCMEN=OFF,WRT=OFF,VCAPEN=OFF,PLLEN=OFF,STVREN=OFF,LVP=OFF

#define  test_bit(var, bit)   ((var) & (1 << (bit)))

char ciklus;
// aproksimativni refresh rate ekrana ce biti 1/(8*red_aktivan_ms*10^-3)
const char red_aktivan_ms = 100; //5 ms u realnoj fizickoj formi
const char obrazac_animirajuceg_prelaza[4][4] = {{255,129,129,129}, {0,126,66,66}, {0,0,60,36}, {0,0,0,24}}; //kvadrat ulazi izlazi animacija pri prelazu
const char obrazac_pri_pobjedi[8] = {0,60,66,129,0,36,36,0}; //smajli emoji
const char obrazac_pri_porazu[8] = {0,129,66,60,0,36,36,0}; //ljutko emoji
const char obrazac_pri_odabiru_tezine[6] = {2,6,14,30,62,126};

//ulaz
__bit pravac_kretanja; //0-x-osa, 1-y-osa
__bit smjer_kretanja; //0-negativno, 1-pozitivno
__bit moguc_novi_ulaz; //da se onemoguci dva puta promjena smijera za jedan pomjeraj zmije

//zmija
char duzina_zmije = 1;
char tromost_zmije = 2;
char pozicije_zmije[8*8][2] = {{16,16}}; // u formi stpen od dva {2^x,2^y} gdje su x y koordinate

//hrana
char nasumicno_sjeme; //sjeme generatora nasumicnih brojeva
char pozicija_hrane[2] = {4,4}; //pozicija hrane takodje u formi stpen od dva {2^x,2^y}
char ciklus_prikazivanja_hrane = 2; //blinkanje hrane da bismo je lakse uocili

//imena funkcija opisuju svoju akciju
void inicijalizacija();
char inicijaliziraj_sjeme();
void odaberi_tezinu_igre();
void ulazne_komande();
char pomjeri_zmiju();
char pomjeri_glavu(char, char);
void generisi_novu_hranu();
void osvjezi_ekran_igre();
void prelazni_animirajuci_ekran();
void prikazi_kraj_igre(char);


void main(void) {
    
    while(1){
        inicijalizacija();
        char stanje_igre = 1; //{0,1,2} 0-poraz, 1-nastavak igre, 2-pobjeda
        ciklus = 1;
        
        prelazni_animirajuci_ekran();
        odaberi_tezinu_igre();
        prelazni_animirajuci_ekran();
        
        while(1){
            ulazne_komande();
            //zmija se pomjera svaki n-ti ciklus osvjezaavanja ekrana
            if(ciklus % tromost_zmije == 0){
                stanje_igre = pomjeri_zmiju();
                //igra zavrsava ako je stanje igre poraz ili pobjeda
                if (stanje_igre == 0 || stanje_igre == 2) break;
            }
            osvjezi_ekran_igre();
            ciklus += 1;
        }
        prelazni_animirajuci_ekran();
        prikazi_kraj_igre(stanje_igre);
    }
    
    return;
}


void ulazne_komande(){
    if(moguc_novi_ulaz && (RD0 || RD1)){
        static __bit B;
        B = RD0 ? 1 : 0;
        //logicka funkcija
        smjer_kretanja = ~pravac_kretanja & ~smjer_kretanja & B | 
                ~pravac_kretanja & smjer_kretanja & ~B | 
                pravac_kretanja & ~smjer_kretanja & ~B | 
                pravac_kretanja & smjer_kretanja & B;
        pravac_kretanja = !pravac_kretanja;
        moguc_novi_ulaz = 0;
    }
}



/*
Ova funkcija efikasno osvežava ekran sa 64 LED diode red po red uz pristojan frame rate (25 ili više). Redovi su povezani na 
8-bitni PORTC mikrokontrolera preko pnp tranzistora, a kolone direktno na 8-bitni PORTB. Za svaki red, ova funkcija skenira kroz 
pozicije zmijinog tijela i hrane s odgovarajućim redom i samo dodaje njihove vrijednosti kolona u varijablu jednoga bajta. Varijabla 
(bitno negirana) se tada samo prosljeđuje na latch LATB koji označava aktivne kolone za taj aktivni red. Mogućnost da se samo dodaju 
kolone pozicije proizilazi iz činjenice da su pohranjene u nizu gdje su prikazane vrijednosti (x i y) u stvari u 'power of two' 
nizu u obliku {{2^x, 2^y},}. Da se ova tehnika ne koristi (pohranjena u {{x, y},}), bilo bi teže postići visok frame rate 
jer bi trebalo računati stepene s funkcijom pow(2, k) mnogo puta u sekundi, što bi bilo prilično skupo (sporo). Sada se pow(2, k) koristi
samo jednom u programu kada inicijalizira nasumičnu poziciju zmije i svaki put kada se nova hrana mora generirati što se rijetko dešava. 
U ovim slučajevima pow(2, k) funkcija se također može zamijeniti lijevim bit shiftingom.
*/
void osvjezi_ekran_igre(){
    char i,k;
    char aktivni_red = 1;
    
    for(i = 0; i < 8; i++){
        ulazne_komande();
        char aktivne_diode_reda = 0x00;
        
        //efikasno rjesenje, samo saberemo kolone jer su u formi stepen od dva
        for(k = 0; k < duzina_zmije; k++)
            if(pozicije_zmije[k][0] == aktivni_red)
                aktivne_diode_reda += pozicije_zmije[k][1];
        
        //takodje saberemo i hranu ako je dosao ciklus prikazivanja, jer treba da trepti dioda gdje je hrana
        if (pozicija_hrane[0] == aktivni_red && ciklus % ciklus_prikazivanja_hrane == 0)
            aktivne_diode_reda += pozicija_hrane[1];
        
        LATC = ~aktivni_red;
        LATB = ~aktivne_diode_reda;
        aktivni_red *= 2;
        __delay_ms(red_aktivan_ms);
    }
}

//prikazuje se animirajuci ekran
void prelazni_animirajuci_ekran(){
    char m,k,i,r;
    
    for(m = 0; m < 2; m++){
        for(k = 0; k < 4; k++){
            r = 0;
            while(r++ < 1){
                char aktivni_red = 1;
                for(i = 0; i < 8; i++){
                    char aktivne_diode_reda = 0x00;
                    aktivne_diode_reda = obrazac_animirajuceg_prelaza[m == 0 ? k : 3-k][i < 4 ? i : 7-i];

                    LATC = ~aktivni_red;
                    LATB = ~aktivne_diode_reda;
                    aktivni_red *= 2;
                    __delay_ms(red_aktivan_ms);
                }
            }
        }
    }
    
}

//prikazuje  se smjesko ili ljutko dok ne pritisnemo neko dugme
void prikazi_kraj_igre(char stanje_igre){
    char i;
    
    while(1){
        if(RD0 || RD1) return;
        char aktivni_red = 1;
        for(i = 0; i < 8; i++){
            char aktivne_diode_reda = 0x00;
            aktivne_diode_reda = stanje_igre == 2 ? obrazac_pri_pobjedi[i] : obrazac_pri_porazu[i];
            
            LATC = ~aktivni_red;
            LATB = ~aktivne_diode_reda;
            aktivni_red *= 2;
            __delay_ms(red_aktivan_ms);
        }
    }
}

char pomjeri_glavu(char smjer, char pozicija){ 
    char v = smjer_kretanja ? pozicija*2 : pozicija/2;
    //prolazi kroz druge strane ekrana takodje
    if(v == 0) v = smjer_kretanja ? 1 : 128;
    return v;
}

//hrana se mora generisati na slobodnim pozicijama i nasumicno izabranoj od njih
void generisi_novu_hranu(){
    char n = rand() % (64-duzina_zmije);
    char c = 0;
    char i, k;
    
    char aktivni_red = 1;
    for(i = 0; i < 8; i++){
        char zmija_na_diodama_reda = 0x00;
        
        for(k = 0; k < duzina_zmije; k++)
            if(pozicije_zmije[k][0] == aktivni_red)
                zmija_na_diodama_reda += pozicije_zmije[k][1];
        
        for(k = 0; k < 8; k++)
            if(!test_bit(zmija_na_diodama_reda, k) && ++c == n){
                pozicija_hrane[0] = aktivni_red;
                pozicija_hrane[1] = pow(2, k);
                return;
            }
                
        aktivni_red *= 2;
    }
}

char inicijaliziraj_sjeme(){
    char sjeme = eeprom_read(0);
    srand(sjeme);
    eeprom_write(0, rand());
}

void inicijalizacija(){
    TRISC = 0x00;
    TRISB = 0x00;
    TRISD = 0x03;
    ANSELD = 0x00;
    
    inicijaliziraj_sjeme();
    pravac_kretanja = rand()%2;
    smjer_kretanja = rand()%2;
    moguc_novi_ulaz = 0;
    
    pozicije_zmije[0][0] = pow(2,rand()%8);
    pozicije_zmije[0][1] = pow(2,rand()%8);
    generisi_novu_hranu();
}


char pomjeri_zmiju(){
    char k;
    char pozicija_kraja[2];
    pozicija_kraja[0] = pozicije_zmije[duzina_zmije-1][0];
    pozicija_kraja[1] = pozicije_zmije[duzina_zmije-1][1];
    
    //pomijeranje tijela zmije
    for(k = duzina_zmije-1; k > 0; k--){
        pozicije_zmije[k][0] = pozicije_zmije[k-1][0];
        pozicije_zmije[k][1] = pozicije_zmije[k-1][1];
    }
    
    //pomijeranje glave zmije
    char red_glave = pozicije_zmije[0][0];
    char kol_glave = pozicije_zmije[0][1];
    pozicije_zmije[0][0] = pravac_kretanja ? pomjeri_glavu(smjer_kretanja, red_glave) : red_glave;
    pozicije_zmije[0][1] = !pravac_kretanja ? pomjeri_glavu(smjer_kretanja, kol_glave) : kol_glave;
    
    //provjera hrane
    if(pozicije_zmije[0][0] == pozicija_hrane[0] && pozicije_zmije[0][1] == pozicija_hrane[1]){
        pozicije_zmije[duzina_zmije][0] = pozicija_kraja[0];
        pozicije_zmije[duzina_zmije][1] = pozicija_kraja[1];
        duzina_zmije += 1;
        generisi_novu_hranu();
    //provjera samoujeda
    }else{
        for(k = 4; k < duzina_zmije; k++){ //moze ujesti tek 4. dio zmije od vrha glave
            if(pozicije_zmije[0][0] == pozicije_zmije[k][0] && pozicije_zmije[0][1] == pozicije_zmije[k][1])
                return 0;
        }
    }
    
    if(!RD0 && !RD1) moguc_novi_ulaz = 1;
    return duzina_zmije == 64 ? 2 : 1;
}

void odaberi_tezinu_igre(){
    char i, slajder = 0;
    static __bit gornja_ivica_desni;
    gornja_ivica_desni = 0;
    
    while(1){
        char aktivni_red = 1;
        for(i = 0; i < 8; i++){
            if(RD1) return;
            if(RD0){
                if(!gornja_ivica_desni){
                    slajder ++;
                    tromost_zmije = 255 - (slajder % 6) * 50;
                }
                gornja_ivica_desni = 1;
            }else if(!RD0) gornja_ivica_desni = 0;
            
            char aktivne_diode_reda = i == 3 || i == 4 ? obrazac_pri_odabiru_tezine[slajder % 6] : 0x00;
            
            LATC = ~aktivni_red;
            LATB = ~aktivne_diode_reda;
            aktivni_red *= 2;
            __delay_ms(red_aktivan_ms);
        }
    }
    
}
