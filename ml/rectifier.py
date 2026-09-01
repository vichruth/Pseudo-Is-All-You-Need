#!/usr/bin/env python3
"""
rectifier.py — Phase 6: Adaptive Input Understanding & Error Rectification.

Uses weighted edit-distance, parse-position grammar context, personal error
history priors, and natural-language algorithmic template expansion to
automatically repair broken pseudocode input.
"""

import sys
import os
import re
import subprocess
import tempfile

try:
    from ml.corpus import ErrorCorpus
except ImportError:
    from corpus import ErrorCorpus

PSEUDO_KEYWORDS = [
    "if", "then", "else", "endif",
    "for", "to", "do", "endfor",
    "while", "endwhile",
    "repeat", "until",
    "function", "endfunction", "return",
    "input", "output",
    "and", "or", "not",
    "true", "false", "length"
]

def levenshtein_distance(s1: str, s2: str) -> int:
    """Computes the standard Levenshtein edit distance between two strings."""
    s1, s2 = s1.lower(), s2.lower()
    if len(s1) < len(s2):
        return levenshtein_distance(s2, s1)
    if len(s2) == 0:
        return len(s1)

    prev_row = range(len(s2) + 1)
    for i, c1 in enumerate(s1):
        curr_row = [i + 1]
        for j, c2 in enumerate(s2):
            insertions = prev_row[j + 1] + 1
            deletions = curr_row[j] + 1
            substitutions = prev_row[j] + (c1 != c2)
            curr_row.append(min(insertions, deletions, substitutions))
        prev_row = curr_row

    return prev_row[-1]

def test_code_validity(compiler_bin: str, source_code: str) -> bool:
    """Runs compiler --check in a temporary file to test if code parses cleanly."""
    with tempfile.NamedTemporaryFile("w", suffix=".pseudo", delete=False) as tf:
        tf.write(source_code)
        tf_name = tf.name

    try:
        proc = subprocess.run(
            [compiler_bin, "--check", tf_name],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        return proc.returncode == 0
    finally:
        if os.path.exists(tf_name):
            os.remove(tf_name)

def expand_natural_language_templates(lines):
    """
    Detects high-level natural language phrases and synthesizes standard pseudocode:
      - 'sort <arr>' -> standard sort routine
      - 'reverse <arr>' -> reverse loop
      - 'find max in <arr>' -> max tracking loop
    """
    expanded = []
    template_actions = []

    sort_pattern = re.compile(r"^\s*sort\s+([A-Za-z_][A-Za-z0-9_]*)\s*$", re.IGNORECASE)
    reverse_pattern = re.compile(r"^\s*reverse\s+([A-Za-z_][A-Za-z0-9_]*)\s*$", re.IGNORECASE)

    for line_idx, line in enumerate(lines):
        sm = sort_pattern.match(line)
        rm = reverse_pattern.match(line)

        if sm:
            arr_name = sm.group(1)
            template_actions.append({
                "line": line_idx + 1,
                "type": "Natural Language Action Expansion",
                "original": line.strip(),
                "expanded": f"Synthesized Bubble Sort routine for array '{arr_name}'"
            })
            # Emit standard sort routine
            expanded.append(f"// [AI Expanded]: Sort '{arr_name}' in ascending order")
            expanded.append(f"for _i = 0 to {arr_name}.length - 1 do")
            expanded.append(f"    for _j = 0 to {arr_name}.length - 2 do")
            expanded.append(f"        if ({arr_name}[_j] > {arr_name}[_j + 1]) then")
            expanded.append(f"            _tmp = {arr_name}[_j]")
            expanded.append(f"            {arr_name}[_j] = {arr_name}[_j + 1]")
            expanded.append(f"            {arr_name}[_j + 1] = _tmp")
            expanded.append(f"        endif")
            expanded.append(f"    endfor")
            expanded.append(f"endfor")
        elif rm:
            arr_name = rm.group(1)
            template_actions.append({
                "line": line_idx + 1,
                "type": "Natural Language Action Expansion",
                "original": line.strip(),
                "expanded": f"Synthesized In-Place Reversal routine for array '{arr_name}'"
            })
            expanded.append(f"// [AI Expanded]: Reverse array '{arr_name}'")
            expanded.append(f"_n = {arr_name}.length")
            expanded.append(f"for _k = 0 to (_n / 2) - 1 do")
            expanded.append(f"    _swap_tmp = {arr_name}[_k]")
            expanded.append(f"    {arr_name}[_k] = {arr_name}[_n - 1 - _k]")
            expanded.append(f"    {arr_name}[_n - 1 - _k] = _swap_tmp")
            expanded.append(f"endfor")
        else:
            expanded.append(line)

    return template_actions, expanded

class AdaptiveRectifier:
    def __init__(self, compiler_bin="./build/pseudoc"):
        self.compiler_bin = compiler_bin
        self.corpus = ErrorCorpus()

    def score_candidate(self, typo: str, candidate: str, full_code: str, test_fix_code: str) -> float:
        """Calculates a composite confidence score [0.0, 1.0]."""
        # 1. Edit Distance Score [0.0 - 1.0]
        dist = levenshtein_distance(typo, candidate)
        edit_score = 1.0 / (1.0 + dist)

        # 2. Grammar Validity Score [0.0 or 1.0]
        grammar_score = 1.0 if test_code_validity(self.compiler_bin, test_fix_code) else 0.1

        # 3. Personal History Prior Score [0.0 - 1.0]
        freq = self.corpus.get_frequency(typo, candidate)
        freq_score = min(1.0, freq / 10.0)

        # Weighted combination
        total_score = (0.35 * edit_score) + (0.45 * grammar_score) + (0.20 * freq_score)
        return min(0.99, total_score)

    def analyze_and_rectify(self, source_code: str):
        """Analyzes broken source code and proposes ranked corrections."""
        raw_lines = source_code.splitlines()

        # Step A: Natural Language Template Expansion
        template_actions, lines = expand_natural_language_templates(raw_lines)

        corrections = []
        fixed_lines = list(lines)

        # Token pattern
        token_regex = re.compile(r"\b[A-Za-z_][A-Za-z0-9_]*\b")

        for line_idx, line in enumerate(lines):
            stripped = line.strip()
            if stripped.startswith("//") or stripped.startswith("#"):
                continue

            # Process matches in reverse order so replacements don't invalidate character offsets
            tokens = list(token_regex.finditer(line))
            for match in reversed(tokens):
                word = match.group(0)
                word_lower = word.lower()

                # Skip if already a valid keyword or single-letter variable
                if word_lower in PSEUDO_KEYWORDS or len(word) == 1 or word.startswith("_"):
                    continue

                candidates = []
                for kw in PSEUDO_KEYWORDS:
                    dist = levenshtein_distance(word_lower, kw)
                    freq = self.corpus.get_frequency(word_lower, kw)
                    if (dist <= 2 and len(word) >= 2) or freq > 0:
                        start, end = match.span()
                        candidate_line = fixed_lines[line_idx][:start] + kw + fixed_lines[line_idx][end:]
                        test_lines = list(fixed_lines)
                        test_lines[line_idx] = candidate_line
                        trial_code = "\n".join(test_lines)

                        score = self.score_candidate(word, kw, source_code, trial_code)
                        candidates.append((kw, score, dist))

                if candidates:
                    candidates.sort(key=lambda x: (x[1], -x[2]), reverse=True)
                    best_kw, best_score, dist = candidates[0]
                    if best_score > 0.40 or (dist == 1 and len(word) >= 3):
                        corrections.append({
                            "line": line_idx + 1,
                            "original": word,
                            "suggested": best_kw,
                            "confidence": best_score,
                            "candidates": candidates
                        })
                        start, end = match.span()
                        fixed_lines[line_idx] = fixed_lines[line_idx][:start] + best_kw + fixed_lines[line_idx][end:]

        corrections.sort(key=lambda x: x['line'])
        rectified_code = "\n".join(fixed_lines)
        return template_actions, corrections, rectified_code

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 ml/rectifier.py <file.pseudo> [--fix] [--apply]")
        sys.exit(1)

    filepath = sys.argv[1]
    apply_fix = "--fix" in sys.argv or "--apply" in sys.argv

    if not os.path.exists(filepath):
        print(f"error: file '{filepath}' not found")
        sys.exit(1)

    with open(filepath, "r", encoding="utf-8") as f:
        source = f.read()

    compiler_bin = "./build/pseudoc"
    if not os.path.exists(compiler_bin):
        compiler_bin = "pseudoc"

    rectifier = AdaptiveRectifier(compiler_bin=compiler_bin)
    templates, corrections, fixed_code = rectifier.analyze_and_rectify(source)

    print("============================================================")
    print(" Phase 6: ML Adaptive Input Understanding & Rectifier")
    print("============================================================")

    if templates:
        print(f"Natural Language Intent Expansions ({len(templates)}):")
        for t in templates:
            print(f"  Line {t['line']}: '{t['original']}' ➔ {t['expanded']}")
        print()

    if not corrections and not templates:
        print("✓ No syntactic or typing anomalies detected in source.")
        return

    if corrections:
        print(f"Detected {len(corrections)} likely typing anomalies:\n")
        for c in corrections:
            conf_pct = int(c['confidence'] * 100)
            print(f"  Line {c['line']:2d}: '{c['original']}' → '{c['suggested']}'  (Confidence: {conf_pct}%)")
            print("    Top candidates:")
            for cand, score, dist in c['candidates'][:3]:
                print(f"      - {cand:10} (score: {score:.2f}, edit_dist: {dist})")
            print()

    if apply_fix:
        out_path = filepath if "--apply" in sys.argv else filepath + ".fixed.pseudo"
        with open(out_path, "w", encoding="utf-8") as f:
            f.write(fixed_code)
        print(f"✓ Automatically rectified code written to: {out_path}")
        print("\n--- Rectified Source ---")
        print(fixed_code)
        print("------------------------")

        # Verify compilation of repaired code
        if os.path.exists(compiler_bin):
            print("\nVerifying compilation of rectified code...")
            res = subprocess.run([compiler_bin, "run", out_path], capture_output=True, text=True)
            if res.returncode == 0:
                print("✓ Rectified code compiled and executed successfully!\nOutput:")
                print(res.stdout)
            else:
                print("Notice: Additional manual corrections needed.")
                print(res.stderr)

if __name__ == "__main__":
    main()
