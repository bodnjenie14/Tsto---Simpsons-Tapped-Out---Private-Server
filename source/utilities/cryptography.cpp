#include "cryptography.hpp"
#include "finally.hpp"
#include "string.hpp"
#include <memory>
#include <cstring>

#ifdef LTM_DESC
extern ltc_math_descriptor ltm_desc;
#endif

namespace utils::cryptography
{
        namespace
        {
                struct __ final
                {
                        __()
                        {
                                register_cipher(&aes_desc);
                                register_prng(&yarrow_desc);
                                register_hash(&sha256_desc);
                                #ifdef LTM_DESC
                                ltc_mp = ltm_desc;
                                #endif
                        }
                } __;

                class prng final
                {
                private:
                        const ltc_prng_descriptor& descriptor_;
                        const int id_;
                        std::unique_ptr<prng_state> state_;

                public:
                        prng(const ltc_prng_descriptor& descriptor, const bool start = true)
                                : descriptor_(descriptor)
                                , id_(register_prng(&descriptor))
                                , state_(std::make_unique<prng_state>())
                        {
                                if (start)
                                {
                                        this->descriptor_.start(this->state_.get());
                                }
                        }

                        ~prng()
                        {
                                this->descriptor_.done(this->state_.get());
                        }

                        prng_state* get_state() const
                        {
                                this->descriptor_.ready(this->state_.get());
                                return this->state_.get();
                        }

                        void auto_seed() const
                        {
                                rng_make_prng(128, this->id_, this->state_.get(), nullptr);
                        }

                        void add_entropy(const void* data, const size_t length) const
                        {
                                this->descriptor_.add_entropy(static_cast<const unsigned char*>(data),
                                        static_cast<unsigned long>(length), this->state_.get());
                        }

                        void read(void* data, const size_t length) const
                        {
                                this->descriptor_.read(static_cast<unsigned char*>(data),
                                        static_cast<unsigned long>(length), this->get_state());
                        }
                };

                const prng prng_(yarrow_desc);
        }

        namespace base64
        {
                std::string encode(const uint8_t* data, const size_t len)
                {
                        if (!data || !len) return {};

                        unsigned long rlen = (len * 2) + 1;
                        std::string result(rlen, 0);

                        if (base64_encode(data, static_cast<unsigned long>(len),
                                reinterpret_cast<unsigned char*>(result.data()), &rlen) != CRYPT_OK)
                        {
                                return {};
                        }

                        result.resize(rlen);
                        return result;
                }

                std::string encode(const std::string& data)
                {
                        return encode(reinterpret_cast<const uint8_t*>(data.data()), data.size());
                }

                std::string decode(const std::string& data)
                {
                        if (data.empty()) return {};

                        unsigned long rlen = data.size() + 1;
                        std::string result(rlen, 0);

                        if (base64_decode(reinterpret_cast<const unsigned char*>(data.data()), 
                                static_cast<unsigned long>(data.size()),
                                reinterpret_cast<unsigned char*>(result.data()), &rlen) != CRYPT_OK)
                        {
                                return {};
                        }

                        result.resize(rlen);
                        return result;
                }
        }

        std::string sha256::compute(const std::string& data, const bool hex)
        {
                return compute(reinterpret_cast<const uint8_t*>(data.data()), data.size(), hex);
        }

        std::string sha256::compute(const uint8_t* data, const size_t length, const bool hex)
        {
                uint8_t buffer[32] = {0};
                hash_state state;
                sha256_init(&state);
                sha256_process(&state, data, static_cast<unsigned long>(length));
                sha256_done(&state, buffer);

                if (!hex) return std::string(reinterpret_cast<const char*>(buffer), sizeof(buffer));
                return utils::string::dump_hex(std::string(reinterpret_cast<const char*>(buffer), sizeof(buffer)));
        }

        std::string md5::compute(const std::string& data, const bool hex)
        {
                return compute(reinterpret_cast<const uint8_t*>(data.data()), data.size(), hex);
        }

        std::string md5::compute(const uint8_t* data, const size_t length, const bool hex)
        {
                uint8_t buffer[16] = {0};
                hash_state state;
                md5_init(&state);
                md5_process(&state, data, static_cast<unsigned long>(length));
                md5_done(&state, buffer);

                if (!hex) return std::string(reinterpret_cast<const char*>(buffer), sizeof(buffer));
                return utils::string::dump_hex(std::string(reinterpret_cast<const char*>(buffer), sizeof(buffer)));
        }

        uint32_t random::get_integer()
        {
                uint32_t result;
                prng_.read(&result, sizeof(result));
                return result;
        }

        uint64_t random::get_longlong()
        {
                uint64_t result;
                prng_.read(&result, sizeof(result));
                return result;
        }

        std::string random::get_challenge()
        {
                uint8_t buffer[32];
                prng_.read(buffer, sizeof(buffer));
                return utils::string::dump_hex(std::string(reinterpret_cast<const char*>(buffer), sizeof(buffer)));
        }

        void random::get_data(void* data, const size_t size)
        {
                prng_.read(data, size);
        }
}
