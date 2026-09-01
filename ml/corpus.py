#!/usr/bin/env python3
"""
corpus.py — Error corpus manager for Phase 6 (Adaptive Input Understanding).

Extracts and tracks typo-to-correction patterns from compiler `.errlog` files
and maintains personal frequency priors in a local SQLite database.
"""

import os
import re
import sqlite3

DEFAULT_DB_PATH = os.path.join(os.path.dirname(__file__), "corpus.db")

COMMON_SEED_TYPOS = [
    ("fr", "for", 15),
    ("fo", "for", 8),
    ("otput", "output", 20),
    ("ouput", "output", 12),
    ("print", "output", 18),
    ("whle", "while", 10),
    ("functon", "function", 14),
    ("func", "function", 16),
    ("def", "function", 10),
    ("retun", "return", 12),
    ("retrun", "return", 9),
    ("thenn", "then", 7),
    ("thn", "then", 5),
    ("ednif", "endif", 8),
    ("end_if", "endif", 6),
    ("ednfor", "endfor", 6),
    ("ednwhile", "endwhile", 5),
    ("untill", "until", 8),
]

class ErrorCorpus:
    def __init__(self, db_path=DEFAULT_DB_PATH):
        self.db_path = db_path
        self.conn = sqlite3.connect(self.db_path)
        self._init_db()

    def _init_db(self):
        with self.conn:
            self.conn.execute("""
                CREATE TABLE IF NOT EXISTS typo_patterns (
                    typo TEXT PRIMARY KEY,
                    correction TEXT,
                    frequency INTEGER DEFAULT 1
                )
            """)
            self.conn.execute("""
                CREATE TABLE IF NOT EXISTS error_history (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    error_code TEXT,
                    phase TEXT,
                    line INTEGER,
                    col INTEGER,
                    message TEXT,
                    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
                )
            """)
        # Seed default common patterns if empty
        cursor = self.conn.cursor()
        cursor.execute("SELECT COUNT(*) FROM typo_patterns")
        if cursor.fetchone()[0] == 0:
            with self.conn:
                for typo, correction, freq in COMMON_SEED_TYPOS:
                    self.conn.execute(
                        "INSERT OR REPLACE INTO typo_patterns (typo, correction, frequency) VALUES (?, ?, ?)",
                        (typo.lower(), correction.lower(), freq)
                    )

    def import_errlog(self, errlog_path=".errlog"):
        """Parses a compiler .errlog file and updates the database."""
        if not os.path.exists(errlog_path):
            return 0

        pattern = re.compile(r"\[(E\d+)\]\s+\[(.*?)\]\s+line\s+(\d+),\s+col\s+(\d+):\s+(.*)")
        imported_count = 0

        with open(errlog_path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                match = pattern.match(line)
                if match:
                    code, phase, l_num, col_num, msg = match.groups()
                    with self.conn:
                        self.conn.execute(
                            "INSERT INTO error_history (error_code, phase, line, col, message) VALUES (?, ?, ?, ?, ?)",
                            (code, phase, int(l_num), int(col_num), msg)
                        )
                    imported_count += 1

        return imported_count

    def record_correction(self, typo, correction):
        """Records a confirmed correction from the user."""
        typo = typo.strip().lower()
        correction = correction.strip().lower()
        with self.conn:
            self.conn.execute("""
                INSERT INTO typo_patterns (typo, correction, frequency)
                VALUES (?, ?, 1)
                ON CONFLICT(typo) DO UPDATE SET
                    correction = excluded.correction,
                    frequency = frequency + 1
            """, (typo, correction))

    def get_frequency(self, typo, correction):
        """Returns the user frequency count for a given typo->correction candidate."""
        cursor = self.conn.cursor()
        cursor.execute(
            "SELECT frequency FROM typo_patterns WHERE typo = ? AND correction = ?",
            (typo.lower(), correction.lower())
        )
        row = cursor.fetchone()
        return row[0] if row else 0

    def get_all_patterns(self):
        cursor = self.conn.cursor()
        cursor.execute("SELECT typo, correction, frequency FROM typo_patterns ORDER BY frequency DESC")
        return cursor.fetchall()

if __name__ == "__main__":
    corpus = ErrorCorpus()
    print(f"Error Corpus initialized at {DEFAULT_DB_PATH}")
    print("Registered patterns:")
    for t, c, f in corpus.get_all_patterns():
        print(f"  {t:10} -> {c:10} (seen {f} times)")
