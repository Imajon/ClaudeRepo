#!/usr/bin/env python3
"""Convertit questions.txt en quiz_data.h (tableau C++ QUIZZES[]), avec
retour a la ligne automatique des questions trop longues pour l'ecran.
Appele automatiquement par compiler-upload-c3-tft-meatboy.bat avant chaque
compilation : modifie questions.txt, le .h est regenere tout seul.
"""
import sys
from pathlib import Path

MAX_CHARS_PER_LINE = 24   # ~ largeur ecran a textSize(2), marge de securite
CAT_IDX = {"maths": 0, "francais": 1, "anglais": 2, "chimie": 3}


def wrap(text, maxlen=MAX_CHARS_PER_LINE):
    if len(text) <= maxlen:
        return text, ""
    words = text.split(" ")
    line1 = ""
    i = 0
    for i, w in enumerate(words):
        cand = (line1 + " " + w).strip()
        if len(cand) > maxlen and line1:
            break
        line1 = cand
    else:
        i += 1
    line2 = " ".join(words[i:])
    return line1, line2


def esc(s):
    return s.replace("\\", "\\\\").replace('"', '\\"')


def parse(path):
    entries = []
    for lineno, raw in enumerate(Path(path).read_text(encoding="utf-8").splitlines(), 1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split("|")]
        if len(parts) == 6: parts.append("2")   # difficulte absente -> 2
        if len(parts) != 7:
            print(f"questions.txt:{lineno}: ignoree (attendu 6-7 champs separes par ' | ', trouve {len(parts)})", file=sys.stderr)
            continue
        cat, q, o1, o2, o3, correct, diff = parts
        cat_l = cat.lower()
        if cat_l not in CAT_IDX:
            print(f"questions.txt:{lineno}: categorie inconnue '{cat}' (maths/francais/anglais/chimie attendu)", file=sys.stderr)
            continue
        try:
            correct_i = int(correct)
        except ValueError:
            print(f"questions.txt:{lineno}: index de reponse invalide '{correct}'", file=sys.stderr)
            continue
        if correct_i not in (0, 1, 2):
            print(f"questions.txt:{lineno}: index de reponse hors 0-2 : {correct_i}", file=sys.stderr)
            continue
        try:
            diff_i = int(diff)
        except ValueError:
            diff_i = 0
        if not 1 <= diff_i <= 5:
            print(f"questions.txt:{lineno}: difficulte invalide '{diff}' (1-5)", file=sys.stderr)
            continue
        entries.append((CAT_IDX[cat_l], q, o1, o2, o3, correct_i, diff_i))
    return entries


def generate(entries):
    lines = [
        "// Fichier GENERE AUTOMATIQUEMENT par gen_quiz.py a partir de questions.txt.",
        "// Ne pas editer a la main : modifier questions.txt puis recompiler.",
        "#pragma once",
        "struct Quiz { uint8_t cat; uint8_t diff; const char* q1; const char* q2; const char* opt[3]; uint8_t correct; };",
        f"#define N_QUIZ {len(entries)}",
        "const Quiz QUIZZES[N_QUIZ] = {",
    ]
    for (cat, q, o1, o2, o3, correct, diff) in entries:
        q1, q2 = wrap(q)
        lines.append(
            f'  {{ {cat}, {diff}, "{esc(q1)}", "{esc(q2)}", '
            f'{{ "{esc(o1)}", "{esc(o2)}", "{esc(o3)}" }}, {correct} }},'
        )
    lines.append("};")
    return "\n".join(lines) + "\n"


def main():
    src = Path(__file__).parent / "questions.txt"
    dst = Path(__file__).parent / "quiz_data.h"
    entries = parse(src)
    if not entries:
        print("Aucune question valide trouvee dans questions.txt -> quiz_data.h non modifie.", file=sys.stderr)
        sys.exit(1)
    dst.write_text(generate(entries), encoding="utf-8")
    print(f"quiz_data.h regenere ({len(entries)} questions).", file=sys.stderr)


if __name__ == "__main__":
    main()
