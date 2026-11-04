/* Verification d'un jeu Berry avant envoi a la console : memes sources et memes reglages que c3-berry.
   Lit le programme sur l'entree standard, le charge, appelle debut() puis 600 images de maj()/dessine() (10 s de jeu,
   boutons appuyes au hasard). Ecrit une ligne JSON : {"ok":true,...} ou {"ok":false,"erreur":"jeu:12: ..."}.
   Compiler : voir compiler-verif.bat (Windows) ou "postinstall" dans package.json (serveur). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "berry.h"
#include "be_vm.h"
#include "be_gc.h"

static char erreur[400]; static int errN = 0;
void berry_sortie(const char *s, size_t n) {       /* print() et erreurs du script */
  for (size_t i = 0; i < n && errN < (int)sizeof erreur - 1; i++) erreur[errN++] = s[i];
  erreur[errN] = 0;
}
static clock_t debutAppel;
static void heartbeat(bvm *v, int ev, ...) {       /* meme garde que la console : un appel ne doit pas tourner 0,5 s */
  if (ev == BE_OBS_VM_HEARTBEAT && clock() - debutAppel > CLOCKS_PER_SEC / 5) be_raise(v, "temps_error", "le script tourne trop longtemps (boucle sans fin ?)");
}
static int ops = 0, txt = 0, bt[3], btAv[3], image = 0;
static int I(bvm *v, int i, int def) { return be_top(v) >= i ? (int)be_toint(v, i) : def; }
static int f_dessin(bvm *v) { ops++; be_return_nil(v); }
static int f_rien(bvm *v) { be_return_nil(v); }
static int f_texte(bvm *v) { ops++; if (be_top(v) >= 3) txt += (int)strlen(be_tostring(v, 3)) + 1; be_return_nil(v); }
static int f_rgb(bvm *v) { int r = I(v, 1, 0), g = I(v, 2, 0), b = I(v, 3, 0); be_pushint(v, ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); be_return(v); }
static int f_bouton(bvm *v) { int k = I(v, 1, 0); be_pushbool(v, k >= 0 && k < 3 && bt[k]); be_return(v); }
static int f_appui(bvm *v) { int k = I(v, 1, 0); be_pushbool(v, k >= 0 && k < 3 && bt[k] && !btAv[k]); be_return(v); }
static int f_lache(bvm *v) { int k = I(v, 1, 0); be_pushbool(v, k >= 0 && k < 3 && !bt[k] && btAv[k]); be_return(v); }
static int f_temps(bvm *v) { be_pushreal(v, 2.0f + image / 60.0f); be_return(v); }
static int f_hasard(bvm *v) { int a = I(v, 1, 0), b = I(v, 2, 0); be_pushint(v, b >= a ? a + rand() % (b - a + 1) : a); be_return(v); }
static int f_envoie(bvm *v) { be_pushbool(v, 1); be_return(v); }
static int f_recois(bvm *v) { be_newobject(v, "list"); be_pop(v, 1); be_return(v); }
static int f_moi(bvm *v) { be_pushint(v, 4242); be_return(v); }

static int appelle(bvm *vm, const char *nom, int argc, float a) {   /* 0 = erreur */
  if (!be_getglobal(vm, nom) || !be_isfunction(vm, -1)) { be_pop(vm, 1); return 1; }
  if (argc) be_pushreal(vm, a);
  debutAppel = clock();
  if (be_pcall(vm, argc)) { errN = 0; be_dumpexcept(vm); return 0; }
  be_pop(vm, argc + 1); return 1;
}
static void json(const char *s) {                   /* chaine JSON echappee */
  putchar('"');
  for (; *s; s++) { if (*s == '"' || *s == '\\') printf("\\%c", *s); else if (*s == '\n') printf("\\n"); else if ((unsigned char)*s < 32) putchar(' '); else putchar(*s); }
  putchar('"');
}
static const char *phase(int i) {                  /* scenario des boutons : ce que fait un enfant qui decouvre le jeu (jamais G+D) */
  memcpy(btAv, bt, sizeof bt);
  bt[0] = bt[1] = bt[2] = 0;
  if (i < 180) { bt[2] = 1; return "P3 tenu"; }
  if (i < 300) { bt[2] = bt[0] = 1; return "P3 + G tenus"; }
  if (i < 420) { bt[2] = bt[1] = 1; return "P3 + D tenus"; }
  if (i < 480) { bt[2] = i % 10 < 3; return "P3 tape plusieurs fois"; }
  if (i < 540) { bt[0] = 1; return "G seul"; }
  bt[1] = 1; return "D seul";
}
static char suivi[4000]; static int suiviN = 0;
static void note(bvm *vm, int argc, char **argv, const char *boutons) {   /* les variables du jeu (nombres) a cet instant */
  suiviN += snprintf(suivi + suiviN, sizeof suivi - suiviN, "%.0f s (%s) :", image / 60.0, boutons);
  for (int i = 1; i < argc && suiviN < (int)sizeof suivi - 40; i++) {
    if (!be_getglobal(vm, argv[i])) { be_pop(vm, 1); continue; }
    if (be_isint(vm, -1)) suiviN += snprintf(suivi + suiviN, sizeof suivi - suiviN, " %s=%d", argv[i], (int)be_toint(vm, -1));
    else if (be_isreal(vm, -1)) suiviN += snprintf(suivi + suiviN, sizeof suivi - suiviN, " %s=%.1f", argv[i], be_toreal(vm, -1));
    else if (be_isbool(vm, -1)) suiviN += snprintf(suivi + suiviN, sizeof suivi - suiviN, " %s=%s", argv[i], be_tobool(vm, -1) ? "true" : "false");
    be_pop(vm, 1);
  }
  if (suiviN < (int)sizeof suivi - 2) suivi[suiviN++] = '\n', suivi[suiviN] = 0;
}
int main(int argc, char **argv) {                  /* argv : les noms des variables du jeu a suivre */
  static char src[65536]; size_t n = fread(src, 1, sizeof src - 1, stdin); src[n] = 0;
  srand(1);
  bvm *vm = be_vm_new(); be_set_obs_hook(vm, heartbeat);
  const char *D[] = { "rect", "cadre", "disque", "cercle", "ligne", "tri", "image" };
  for (int i = 0; i < 7; i++) be_regfunc(vm, D[i], f_dessin);
  be_regfunc(vm, "fond", f_rien); be_regfunc(vm, "camera", f_rien); be_regfunc(vm, "son", f_rien); be_regfunc(vm, "score", f_rien);
  be_regfunc(vm, "texte", f_texte); be_regfunc(vm, "rgb", f_rgb); be_regfunc(vm, "bouton", f_bouton); be_regfunc(vm, "appui", f_appui);
  be_regfunc(vm, "lache", f_lache); be_regfunc(vm, "temps", f_temps); be_regfunc(vm, "hasard", f_hasard);
  be_regfunc(vm, "envoie", f_envoie); be_regfunc(vm, "recois", f_recois); be_regfunc(vm, "moi", f_moi);
  const char *K[3] = { "G", "D", "P3" }; for (int k = 0; k < 3; k++) { be_pushint(vm, k); be_setglobal(vm, K[k]); be_pop(vm, 1); }
  debutAppel = clock();
  int ok = !(be_loadbuffer(vm, "jeu", src, n) || be_pcall(vm, 0));
  if (!ok) { errN = 0; be_dumpexcept(vm); }
  else { be_pop(vm, 1); ok = appelle(vm, "debut", 0, 0); }
  int opsMax = 0, txtMax = 0; size_t memMax = 0; uint32_t ins0 = vm->counter_ins;
  for (image = 0; ok && image < 600; image++) {
    const char *b = phase(image);
    if (image % 60 == 0) note(vm, argc, argv, b);
    ops = txt = 0;
    ok = appelle(vm, "maj", 1, 1 / 60.0f) && appelle(vm, "dessine", 0, 0);
    if (ops > opsMax) opsMax = ops;
    if (txt > txtMax) txtMax = txt;
    size_t m = be_gc_memcount(vm); if (m > memMax) memMax = m;
  }
  printf("{\"ok\":%s,\"images\":%d,\"ops\":%d,\"texte\":%d,\"memoire\":%u,\"instructions\":%u",
         ok ? "true" : "false", image, opsMax, txtMax, (unsigned)memMax, image ? (unsigned)((vm->counter_ins - ins0) / image) : 0);
  if (ok) note(vm, argc, argv, "fin");
  printf(",\"suivi\":"); json(suivi);
  if (!ok) { printf(",\"erreur\":"); json(erreur); }
  printf("}\n");
  return 0;
}
