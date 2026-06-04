template <size_t NBits>
class Bitset {
public:
    static constexpr size_t WordCount = (NBits + (WordBits - 1)) / WordBits;
    using Word = uint64_t; // or uint32_t depending on target

    constexpr Bitset() noexcept = default;

    void Set(size_t bit) noexcept;
    void Reset(size_t bit) noexcept;
    bool Test(size_t bit) const noexcept;
    void SetAll() noexcept;
    void ResetAll() noexcept;

    // Bitwise operations
    Bitset& operator|=(const Bitset& rhs) noexcept;
    Bitset& operator&=(const Bitset& rhs) noexcept;
    Bitset  operator~() const noexcept;

    bool Any() const noexcept;
    bool All() const noexcept;
    size_t Count() const noexcept; // popcount

private:
    std::array<Word, WordCount> words_{};
};
