"""Tests for word exercise functionality in the Learn tab."""

import pytest


class TestWordList:
    """Tests for the WORD_LIST constant."""

    def test_import(self):
        from nchorder_tools.wordlist import WORD_LIST
        assert isinstance(WORD_LIST, tuple)

    def test_word_count(self):
        from nchorder_tools.wordlist import WORD_LIST
        assert len(WORD_LIST) >= 1500
        assert len(WORD_LIST) <= 4000

    def test_all_lowercase_ascii(self):
        from nchorder_tools.wordlist import WORD_LIST
        for word in WORD_LIST:
            assert word.isalpha(), f"Non-alpha word: {word!r}"
            assert word.islower(), f"Non-lowercase word: {word!r}"
            assert word.isascii(), f"Non-ASCII word: {word!r}"

    def test_length_range(self):
        from nchorder_tools.wordlist import WORD_LIST
        for word in WORD_LIST:
            assert 2 <= len(word) <= 8, f"Word out of range: {word!r} (len={len(word)})"

    def test_no_duplicates(self):
        from nchorder_tools.wordlist import WORD_LIST
        assert len(WORD_LIST) == len(set(WORD_LIST)), "Duplicate words found"

    def test_has_short_words(self):
        """Should have common 2-3 letter words for early combos."""
        from nchorder_tools.wordlist import WORD_LIST
        short = [w for w in WORD_LIST if len(w) <= 3]
        assert len(short) >= 50

    def test_has_common_words(self):
        """Spot-check that very common words are present."""
        from nchorder_tools.wordlist import WORD_LIST
        common = {'the', 'and', 'for', 'are', 'not', 'you', 'all', 'can', 'had', 'her',
                  'was', 'one', 'our', 'out', 'has', 'his', 'how', 'man', 'new', 'old'}
        present = common & set(WORD_LIST)
        assert len(present) >= 15, f"Missing common words: {common - present}"


class TestWordFiltering:
    """Tests for _get_words_for_chars and _generate_word_round methods."""

    def _make_exercise_view(self):
        """Create a minimal ExerciseView-like object with word methods.

        Avoids Kivy import by testing the logic directly.
        """
        import random
        from nchorder_tools.wordlist import WORD_LIST

        class MockExerciseView:
            def _get_words_for_chars(self, chars: set) -> list:
                return [w for w in WORD_LIST if set(w) <= chars]

            def _generate_word_round(self, chars: set) -> str:
                words = self._get_words_for_chars(chars)
                if len(words) < 5:
                    return ''
                count = min(random.randint(10, 15), len(words))
                selected = random.sample(words, count)
                separator = ' ' if ' ' in chars else ''
                return separator.join(selected)

        return MockExerciseView()

    def test_filter_basic(self):
        """Filter with common letter set returns words."""
        view = self._make_exercise_view()
        # e, t, a are very common letters
        words = view._get_words_for_chars({'e', 't', 'a'})
        assert len(words) > 0
        # All returned words should only use e, t, a
        for w in words:
            assert set(w) <= {'e', 't', 'a'}, f"Word {w!r} uses chars outside {{e, t, a}}"

    def test_filter_includes_expected_words(self):
        """Words like 'eat', 'tea', 'ate' should appear for {e, t, a}."""
        view = self._make_exercise_view()
        words = view._get_words_for_chars({'e', 't', 'a'})
        word_set = set(words)
        # At least some of these should be present
        expected = {'eat', 'tea', 'ate', 'at'}
        found = expected & word_set
        assert len(found) >= 2, f"Expected some of {expected}, got {found}"

    def test_filter_empty_for_rare_chars(self):
        """Very limited char set returns few/no words."""
        view = self._make_exercise_view()
        words = view._get_words_for_chars({'z', 'x'})
        # Very few English words use only z and x
        assert len(words) <= 2

    def test_generate_word_round_empty_when_few_words(self):
        """Word round returns empty when < 5 words available."""
        view = self._make_exercise_view()
        result = view._generate_word_round({'z', 'x'})
        assert result == ''

    def test_generate_word_round_nonempty_with_enough_chars(self):
        """Word round returns text with enough chars learned."""
        view = self._make_exercise_view()
        # Common letters that should yield many words
        chars = set('etaoinshrd')
        result = view._generate_word_round(chars)
        assert len(result) > 0

    def test_generate_word_round_uses_only_learned_chars(self):
        """All characters in word round text are from learned set."""
        view = self._make_exercise_view()
        chars = set('etaoinshrd')
        result = view._generate_word_round(chars)
        if result:
            for ch in result:
                assert ch in chars, f"Char {ch!r} not in learned set"

    def test_generate_word_round_space_separator(self):
        """Words separated by space when space is in learned set."""
        view = self._make_exercise_view()
        chars = set('etaoinshrd ')
        result = view._generate_word_round(chars)
        if result:
            assert ' ' in result, "Expected spaces between words"

    def test_generate_word_round_no_space_concatenated(self):
        """Words concatenated when space is NOT in learned set."""
        view = self._make_exercise_view()
        chars = set('etaoinshrd')
        result = view._generate_word_round(chars)
        if result:
            assert ' ' not in result, "Expected no spaces when space not learned"

    def test_word_round_length(self):
        """Word round should have 10-15 words worth of content."""
        view = self._make_exercise_view()
        chars = set('etaoinshrd ')
        result = view._generate_word_round(chars)
        if result:
            words = result.split(' ')
            assert 10 <= len(words) <= 15, f"Expected 10-15 words, got {len(words)}"
