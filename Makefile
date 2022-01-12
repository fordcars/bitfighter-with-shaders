#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
# GRAPHICS is a list of directories containing graphics files
# GFXBUILD is the directory where converted graphics files will be placed
#   If set to $(BUILD), it will statically link in the converted
#   files as if they were data files.
#
# NO_SMDH: if set to anything, no SMDH file is generated.
# ROMFS is the directory which contains the RomFS, relative to the Makefile (Optional)
# APP_TITLE is the name of the app stored in the SMDH file (Optional)
# APP_DESCRIPTION is the description of the app stored in the SMDH file (Optional)
# APP_AUTHOR is the author of the app stored in the SMDH file (Optional)
# ICON is the filename of the icon (.png), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.png
#     - icon.png
#     - <libctru folder>/default_icon.png
#---------------------------------------------------------------------------------
TARGET		:=	$(notdir $(CURDIR))
BUILD		:=	build
SOURCES		:=	zap clipper fontstash poly2tri/sweep poly2tri/common tnl tnlping tomcrypt/src/pk/rsa/rsa_decrypt_key.c tomcrypt/src/pk/rsa/rsa_verify_hash.c tomcrypt/src/pk/rsa/rsa_sign_hash.c tomcrypt/src/pk/rsa/rsa_free.c tomcrypt/src/pk/rsa/rsa_make_key.c tomcrypt/src/pk/rsa/rsa_export.c tomcrypt/src/pk/rsa/rsa_encrypt_key.c tomcrypt/src/pk/rsa/rsa_exptmod.c tomcrypt/src/pk/rsa/rsa_import.c tomcrypt/src/pk/pkcs1/pkcs_1_mgf1.c tomcrypt/src/pk/pkcs1/pkcs_1_v1_5_encode.c tomcrypt/src/pk/pkcs1/pkcs_1_i2osp.c tomcrypt/src/pk/pkcs1/pkcs_1_pss_decode.c tomcrypt/src/pk/pkcs1/pkcs_1_os2ip.c tomcrypt/src/pk/pkcs1/pkcs_1_oaep_decode.c tomcrypt/src/pk/pkcs1/pkcs_1_v1_5_decode.c tomcrypt/src/pk/pkcs1/pkcs_1_pss_encode.c tomcrypt/src/pk/pkcs1/pkcs_1_oaep_encode.c tomcrypt/src/pk/katja/katja_exptmod.c tomcrypt/src/pk/katja/katja_encrypt_key.c tomcrypt/src/pk/katja/katja_import.c tomcrypt/src/pk/katja/katja_make_key.c tomcrypt/src/pk/katja/katja_decrypt_key.c tomcrypt/src/pk/katja/katja_free.c tomcrypt/src/pk/katja/katja_export.c tomcrypt/src/pk/dsa/dsa_sign_hash.c tomcrypt/src/pk/dsa/dsa_decrypt_key.c tomcrypt/src/pk/dsa/dsa_encrypt_key.c tomcrypt/src/pk/dsa/dsa_verify_key.c tomcrypt/src/pk/dsa/dsa_verify_hash.c tomcrypt/src/pk/dsa/dsa_shared_secret.c tomcrypt/src/pk/dsa/dsa_import.c tomcrypt/src/pk/dsa/dsa_free.c tomcrypt/src/pk/dsa/dsa_make_key.c tomcrypt/src/pk/dsa/dsa_export.c tomcrypt/src/pk/ecc/ltc_ecc_points.c tomcrypt/src/pk/ecc/ecc_ansi_x963_export.c tomcrypt/src/pk/ecc/ecc_make_key.c tomcrypt/src/pk/ecc/ecc_get_size.c tomcrypt/src/pk/ecc/ecc_sign_hash.c tomcrypt/src/pk/ecc/ltc_ecc_projective_add_point.c tomcrypt/src/pk/ecc/ecc_free.c tomcrypt/src/pk/ecc/ecc_shared_secret.c tomcrypt/src/pk/ecc/ecc_sizes.c tomcrypt/src/pk/ecc/ltc_ecc_mulmod.c tomcrypt/src/pk/ecc/ltc_ecc_mul2add.c tomcrypt/src/pk/ecc/ecc_export.c tomcrypt/src/pk/ecc/ecc.c tomcrypt/src/pk/ecc/ltc_ecc_is_valid_idx.c tomcrypt/src/pk/ecc/ltc_ecc_map.c tomcrypt/src/pk/ecc/ecc_verify_hash.c tomcrypt/src/pk/ecc/ecc_decrypt_key.c tomcrypt/src/pk/ecc/ecc_test.c tomcrypt/src/pk/ecc/ltc_ecc_mulmod_timing.c tomcrypt/src/pk/ecc/ecc_ansi_x963_import.c tomcrypt/src/pk/ecc/ecc_encrypt_key.c tomcrypt/src/pk/ecc/ltc_ecc_projective_dbl_point.c tomcrypt/src/pk/ecc/ecc_import.c tomcrypt/src/pk/asn1/der/utctime/der_encode_utctime.c tomcrypt/src/pk/asn1/der/utctime/der_decode_utctime.c tomcrypt/src/pk/asn1/der/utctime/der_length_utctime.c tomcrypt/src/pk/asn1/der/utf8/der_encode_utf8_string.c tomcrypt/src/pk/asn1/der/utf8/der_decode_utf8_string.c tomcrypt/src/pk/asn1/der/utf8/der_length_utf8_string.c tomcrypt/src/pk/asn1/der/integer/der_encode_integer.c tomcrypt/src/pk/asn1/der/integer/der_decode_integer.c tomcrypt/src/pk/asn1/der/integer/der_length_integer.c tomcrypt/src/pk/asn1/der/printable_string/der_decode_printable_string.c tomcrypt/src/pk/asn1/der/printable_string/der_encode_printable_string.c tomcrypt/src/pk/asn1/der/printable_string/der_length_printable_string.c tomcrypt/src/pk/asn1/der/object_identifier/der_decode_object_identifier.c tomcrypt/src/pk/asn1/der/object_identifier/der_encode_object_identifier.c tomcrypt/src/pk/asn1/der/object_identifier/der_length_object_identifier.c tomcrypt/src/pk/asn1/der/boolean/der_decode_boolean.c tomcrypt/src/pk/asn1/der/boolean/der_encode_boolean.c tomcrypt/src/pk/asn1/der/boolean/der_length_boolean.c tomcrypt/src/pk/asn1/der/bit/der_decode_bit_string.c tomcrypt/src/pk/asn1/der/bit/der_encode_bit_string.c tomcrypt/src/pk/asn1/der/bit/der_length_bit_string.c tomcrypt/src/pk/asn1/der/bit/der_decode_raw_bit_string.c tomcrypt/src/pk/asn1/der/bit/der_encode_raw_bit_string.c tomcrypt/src/pk/asn1/der/teletex_string/der_decode_teletex_string.c tomcrypt/src/pk/asn1/der/teletex_string/der_length_teletex_string.c tomcrypt/src/pk/asn1/der/generalizedtime/der_decode_generalizedtime.c tomcrypt/src/pk/asn1/der/generalizedtime/der_encode_generalizedtime.c tomcrypt/src/pk/asn1/der/generalizedtime/der_length_generalizedtime.c tomcrypt/src/pk/asn1/der/set/der_encode_set.c tomcrypt/src/pk/asn1/der/set/der_encode_setof.c tomcrypt/src/pk/asn1/der/octet/der_length_octet_string.c tomcrypt/src/pk/asn1/der/octet/der_encode_octet_string.c tomcrypt/src/pk/asn1/der/octet/der_decode_octet_string.c tomcrypt/src/pk/asn1/der/sequence/der_decode_sequence_flexi.c tomcrypt/src/pk/asn1/der/sequence/der_length_sequence.c tomcrypt/src/pk/asn1/der/sequence/der_decode_sequence_multi.c tomcrypt/src/pk/asn1/der/sequence/der_sequence_free.c tomcrypt/src/pk/asn1/der/sequence/der_decode_sequence_ex.c tomcrypt/src/pk/asn1/der/sequence/der_encode_sequence_multi.c tomcrypt/src/pk/asn1/der/sequence/der_encode_sequence_ex.c tomcrypt/src/pk/asn1/der/ia5/der_length_ia5_string.c tomcrypt/src/pk/asn1/der/ia5/der_decode_ia5_string.c tomcrypt/src/pk/asn1/der/ia5/der_encode_ia5_string.c tomcrypt/src/pk/asn1/der/short_integer/der_encode_short_integer.c tomcrypt/src/pk/asn1/der/short_integer/der_length_short_integer.c tomcrypt/src/pk/asn1/der/short_integer/der_decode_short_integer.c tomcrypt/src/pk/asn1/der/choice/der_decode_choice.c tomcrypt/src/hashes/tiger.c tomcrypt/src/hashes/rmd128.c tomcrypt/src/hashes/chc/chc.c tomcrypt/src/hashes/rmd256.c tomcrypt/src/hashes/sha2/sha512.c tomcrypt/src/hashes/sha2/sha256.c tomcrypt/src/hashes/sha2/sha384.c tomcrypt/src/hashes/sha2/sha224.c tomcrypt/src/hashes/sha1.c tomcrypt/src/hashes/md5.c tomcrypt/src/hashes/md4.c tomcrypt/src/hashes/md2.c tomcrypt/src/hashes/rmd320.c tomcrypt/src/hashes/whirl/whirltab.c tomcrypt/src/hashes/whirl/whirl.c tomcrypt/src/hashes/rmd160.c tomcrypt/src/hashes/helper/hash_memory.c tomcrypt/src/hashes/helper/hash_memory_multi.c tomcrypt/src/hashes/helper/hash_file.c tomcrypt/src/hashes/helper/hash_filehandle.c tomcrypt/src/modes/ctr/ctr_done.c tomcrypt/src/modes/ctr/ctr_getiv.c tomcrypt/src/modes/ctr/ctr_encrypt.c tomcrypt/src/modes/ctr/ctr_start.c tomcrypt/src/modes/ctr/ctr_test.c tomcrypt/src/modes/ctr/ctr_setiv.c tomcrypt/src/modes/ctr/ctr_decrypt.c tomcrypt/src/modes/xts/xts_mult_x.c tomcrypt/src/modes/xts/xts_done.c tomcrypt/src/modes/xts/xts_encrypt.c tomcrypt/src/modes/xts/xts_test.c tomcrypt/src/modes/xts/xts_decrypt.c tomcrypt/src/modes/xts/xts_init.c tomcrypt/src/modes/cfb/cfb_encrypt.c tomcrypt/src/modes/cfb/cfb_decrypt.c tomcrypt/src/modes/cfb/cfb_getiv.c tomcrypt/src/modes/cfb/cfb_start.c tomcrypt/src/modes/cfb/cfb_setiv.c tomcrypt/src/modes/cfb/cfb_done.c tomcrypt/src/modes/lrw/lrw_start.c tomcrypt/src/modes/lrw/lrw_getiv.c tomcrypt/src/modes/lrw/lrw_done.c tomcrypt/src/modes/lrw/lrw_setiv.c tomcrypt/src/modes/lrw/lrw_decrypt.c tomcrypt/src/modes/lrw/lrw_test.c tomcrypt/src/modes/lrw/lrw_encrypt.c tomcrypt/src/modes/lrw/lrw_process.c tomcrypt/src/modes/f8/f8_setiv.c tomcrypt/src/modes/f8/f8_encrypt.c tomcrypt/src/modes/f8/f8_test_mode.c tomcrypt/src/modes/f8/f8_done.c tomcrypt/src/modes/f8/f8_getiv.c tomcrypt/src/modes/f8/f8_start.c tomcrypt/src/modes/f8/f8_decrypt.c tomcrypt/src/modes/cbc/cbc_setiv.c tomcrypt/src/modes/cbc/cbc_getiv.c tomcrypt/src/modes/cbc/cbc_done.c tomcrypt/src/modes/cbc/cbc_start.c tomcrypt/src/modes/cbc/cbc_decrypt.c tomcrypt/src/modes/cbc/cbc_encrypt.c tomcrypt/src/modes/ofb/ofb_start.c tomcrypt/src/modes/ofb/ofb_done.c tomcrypt/src/modes/ofb/ofb_encrypt.c tomcrypt/src/modes/ofb/ofb_setiv.c tomcrypt/src/modes/ofb/ofb_getiv.c tomcrypt/src/modes/ofb/ofb_decrypt.c tomcrypt/src/modes/ecb/ecb_encrypt.c tomcrypt/src/modes/ecb/ecb_decrypt.c tomcrypt/src/modes/ecb/ecb_start.c tomcrypt/src/modes/ecb/ecb_done.c tomcrypt/src/mac/pelican/pelican.c tomcrypt/src/mac/pelican/pelican_memory.c tomcrypt/src/mac/pelican/pelican_test.c tomcrypt/src/mac/pmac/pmac_shift_xor.c tomcrypt/src/mac/pmac/pmac_file.c tomcrypt/src/mac/pmac/pmac_done.c tomcrypt/src/mac/pmac/pmac_memory.c tomcrypt/src/mac/pmac/pmac_process.c tomcrypt/src/mac/pmac/pmac_init.c tomcrypt/src/mac/pmac/pmac_memory_multi.c tomcrypt/src/mac/pmac/pmac_ntz.c tomcrypt/src/mac/pmac/pmac_test.c tomcrypt/src/mac/hmac/hmac_memory_multi.c tomcrypt/src/mac/hmac/hmac_process.c tomcrypt/src/mac/hmac/hmac_test.c tomcrypt/src/mac/hmac/hmac_file.c tomcrypt/src/mac/hmac/hmac_done.c tomcrypt/src/mac/hmac/hmac_memory.c tomcrypt/src/mac/hmac/hmac_init.c tomcrypt/src/mac/f9/f9_init.c tomcrypt/src/mac/f9/f9_file.c tomcrypt/src/mac/f9/f9_done.c tomcrypt/src/mac/f9/f9_memory_multi.c tomcrypt/src/mac/f9/f9_memory.c tomcrypt/src/mac/f9/f9_test.c tomcrypt/src/mac/f9/f9_process.c tomcrypt/src/mac/xcbc/xcbc_init.c tomcrypt/src/mac/xcbc/xcbc_file.c tomcrypt/src/mac/xcbc/xcbc_memory_multi.c tomcrypt/src/mac/xcbc/xcbc_test.c tomcrypt/src/mac/xcbc/xcbc_memory.c tomcrypt/src/mac/xcbc/xcbc_process.c tomcrypt/src/mac/xcbc/xcbc_done.c tomcrypt/src/mac/omac/omac_done.c tomcrypt/src/mac/omac/omac_memory_multi.c tomcrypt/src/mac/omac/omac_process.c  tomcrypt/src/mac/omac/omac_memory.ctomcrypt/src/mac/omac/omac_test.c tomcrypt/src/mac/omac/omac_init.c tomcrypt/src/mac/omac/omac_file.c tomcrypt/src/misc/crypt/crypt_find_hash_any.c tomcrypt/src/misc/crypt/crypt_register_cipher.c tomcrypt/src/misc/crypt/crypt_prng_is_valid.c tomcrypt/src/misc/crypt/crypt_unregister_hash.c tomcrypt/src/misc/crypt/crypt_argchk.c tomcrypt/src/misc/crypt/crypt_unregister_prng.c tomcrypt/src/misc/crypt/crypt_cipher_descriptor.c tomcrypt/src/misc/crypt/crypt_fsa.c tomcrypt/src/misc/crypt/crypt_find_hash_oid.c tomcrypt/src/misc/crypt/crypt_find_prng.c tomcrypt/src/misc/crypt/crypt_register_prng.c tomcrypt/src/misc/crypt/crypt_ltc_mp_descriptor.c tomcrypt/src/misc/crypt/crypt.c tomcrypt/src/misc/crypt/crypt_unregister_cipher.c tomcrypt/src/misc/crypt/crypt_find_cipher_any.c tomcrypt/src/misc/crypt/crypt_find_hash_id.c tomcrypt/src/misc/crypt/crypt_find_cipher_id.c tomcrypt/src/misc/crypt/crypt_find_cipher.c tomcrypt/src/misc/crypt/crypt_prng_descriptor.c tomcrypt/src/misc/crypt/crypt_find_hash.c tomcrypt/src/misc/crypt/crypt_register_hash.c tomcrypt/src/misc/crypt/crypt_hash_is_valid.c tomcrypt/src/misc/crypt/crypt_hash_descriptor.c tomcrypt/src/misc/crypt/crypt_cipher_is_valid.c tomcrypt/src/misc/zeromem.c tomcrypt/src/misc/burn_stack.c tomcrypt/src/misc/pkcs5/pkcs_5_2.c tomcrypt/src/misc/pkcs5/pkcs_5_1.c tomcrypt/src/misc/error_to_string.c tomcrypt/src/misc/base64/base64_decode.c tomcrypt/src/misc/base64/base64_encode.c tomcrypt/src/prngs/sober128tab.c tomcrypt/src/prngs/yarrow.c tomcrypt/src/prngs/sprng.c tomcrypt/src/prngs/rng_get_bytes.c tomcrypt/src/prngs/sober128.c tomcrypt/src/prngs/rc4.c tomcrypt/src/prngs/fortuna.c tomcrypt/src/prngs/rng_make_prng.c tomcrypt/src/math/tfm_desc.c tomcrypt/src/math/ltm_desc.c tomcrypt/src/math/fp/ltc_ecc_fp_mulmod.c tomcrypt/src/math/multi.c tomcrypt/src/math/rand_prime.c tomcrypt/src/math/gmp_desc.c tomcrypt/src/ciphers/rc6.c tomcrypt/src/ciphers/anubis.c tomcrypt/src/ciphers/rc5.c tomcrypt/src/ciphers/xtea.c tomcrypt/src/ciphers/noekeon.c tomcrypt/src/ciphers/des.c tomcrypt/src/ciphers/cast5.c tomcrypt/src/ciphers/multi2.c tomcrypt/src/ciphers/rc2.c tomcrypt/src/ciphers/safer/safer_tab.c tomcrypt/src/ciphers/safer/safer.c tomcrypt/src/ciphers/safer/saferp.c tomcrypt/src/ciphers/khazad.c tomcrypt/src/ciphers/aes/aes_tab.c tomcrypt/src/ciphers/aes/aes_enc.c tomcrypt/src/ciphers/aes/aes.c tomcrypt/src/ciphers/kasumi.c tomcrypt/src/ciphers/skipjack.c tomcrypt/src/ciphers/kseed.c tomcrypt/src/ciphers/blowfish.c tomcrypt/src/ciphers/twofish/twofish_tab.c tomcrypt/src/ciphers/twofish/twofish.c tomcrypt/src/encauth/ccm/ccm_memory.c tomcrypt/src/encauth/ccm/ccm_test.c tomcrypt/src/encauth/gcm/gcm_init.c tomcrypt/src/encauth/gcm/gcm_gf_mult.c tomcrypt/src/encauth/gcm/gcm_process.c tomcrypt/src/encauth/gcm/gcm_done.c tomcrypt/src/encauth/gcm/gcm_test.c tomcrypt/src/encauth/gcm/gcm_mult_h.c tomcrypt/src/encauth/gcm/gcm_add_aad.c tomcrypt/src/encauth/gcm/gcm_memory.c tomcrypt/src/encauth/gcm/gcm_add_iv.c tomcrypt/src/encauth/gcm/gcm_reset.c tomcrypt/src/encauth/eax/eax_addheader.c tomcrypt/src/encauth/eax/eax_decrypt_verify_memory.c tomcrypt/src/encauth/eax/eax_init.c tomcrypt/src/encauth/eax/eax_encrypt_authenticate_memory.c tomcrypt/src/encauth/eax/eax_decrypt.c tomcrypt/src/encauth/eax/eax_done.c tomcrypt/src/encauth/eax/eax_encrypt.c tomcrypt/src/encauth/eax/eax_test.c tomcrypt/src/encauth/ocb/s_ocb_done.c tomcrypt/src/encauth/ocb/ocb_encrypt_authenticate_memory.c tomcrypt/src/encauth/ocb/ocb_decrypt.c tomcrypt/src/encauth/ocb/ocb_test.c tomcrypt/src/encauth/ocb/ocb_init.c tomcrypt/src/encauth/ocb/ocb_done_encrypt.c tomcrypt/src/encauth/ocb/ocb_done_decrypt.c tomcrypt/src/encauth/ocb/ocb_decrypt_verify_memory.c tomcrypt/src/encauth/ocb/ocb_shift_xor.c tomcrypt/src/encauth/ocb/ocb_ntz.c tomcrypt/src/encauth/ocb/ocb_encrypt.c
SOURCES     := $(SOURCES) 
DATA		:=	data
INCLUDES	:=	SDL zap clipper fontstash poly2tri poly2tri/sweep poly2tri/common tnl tnlping tomcrypt/src/headers lua/luajit/src
GRAPHICS	:=	gfx
GFXBUILD	:=	$(BUILD)
#ROMFS		:=	romfs
#GFXBUILD	:=	$(ROMFS)/gfx

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -O2 -mword-relocations \
			-ffunction-sections \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -Iinclude -D__3DS__ -DBF_PLATFORM_3DS -DBF_NO_AUDIO -Dlinux -DTNL_NO_THREADS

CXXFLAGS	:= $(CFLAGS) -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lcitro2d -lcitro3d -lctru -lm -lsdl

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(CTRULIB) $(PORTLIBS)


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))
GFXFILES	:=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
ifeq ($(GFXBUILD),$(BUILD))
#---------------------------------------------------------------------------------
export T3XFILES :=  $(GFXFILES:.t3s=.t3x)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
export ROMFS_T3XFILES	:=	$(patsubst %.t3s, $(GFXBUILD)/%.t3x, $(GFXFILES))
export T3XHFILES		:=	$(patsubst %.t3s, $(BUILD)/%.h, $(GFXFILES))
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_SOURCES 	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES)) \
			$(PICAFILES:.v.pica=.shbin.o) $(SHLISTFILES:.shlist=.shbin.o) \
			$(addsuffix .o,$(T3XFILES))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES	:=	$(PICAFILES:.v.pica=_shbin.h) $(SHLISTFILES:.shlist=_shbin.h) \
			$(addsuffix .h,$(subst .,_,$(BINFILES))) \
			$(GFXFILES:.t3s=.h)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS	:=	$(if $(NO_SMDH),,$(OUTPUT).smdh)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: all clean

#---------------------------------------------------------------------------------
all: $(BUILD) $(GFXBUILD) $(DEPSDIR) $(ROMFS_T3XFILES) $(T3XHFILES)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

ifneq ($(GFXBUILD),$(BUILD))
$(GFXBUILD):
	@mkdir -p $@
endif

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf $(GFXBUILD)

#---------------------------------------------------------------------------------
$(GFXBUILD)/%.t3x	$(BUILD)/%.h	:	%.t3s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@tex3ds -i $< -H $(BUILD)/$*.h -d $(DEPSDIR)/$*.d -o $(GFXBUILD)/$*.t3x

#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).3dsx	:	$(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf	:	$(OFILES)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
.PRECIOUS	:	%.t3x
#---------------------------------------------------------------------------------
%.t3x.o	%_t3x.h :	%.t3x
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
# rules for assembling GPU shaders
#---------------------------------------------------------------------------------
define shader-as
	$(eval CURBIN := $*.shbin)
	$(eval DEPSFILE := $(DEPSDIR)/$*.shbin.d)
	echo "$(CURBIN).o: $< $1" > $(DEPSFILE)
	echo "extern const u8" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"_end[];" > `(echo $(CURBIN) | tr . _)`.h
	echo "extern const u8" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`"[];" >> `(echo $(CURBIN) | tr . _)`.h
	echo "extern const u32" `(echo $(CURBIN) | sed -e 's/^\([0-9]\)/_\1/' | tr . _)`_size";" >> `(echo $(CURBIN) | tr . _)`.h
	picasso -o $(CURBIN) $1
	bin2s $(CURBIN) | $(AS) -o $*.shbin.o
endef

%.shbin.o %_shbin.h : %.v.pica %.g.pica
	@echo $(notdir $^)
	@$(call shader-as,$^)

%.shbin.o %_shbin.h : %.v.pica
	@echo $(notdir $<)
	@$(call shader-as,$<)

%.shbin.o %_shbin.h : %.shlist
	@echo $(notdir $<)
	@$(call shader-as,$(foreach file,$(shell cat $<),$(dir $<)$(file)))

#---------------------------------------------------------------------------------
%.t3x	%.h	:	%.t3s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@tex3ds -i $< -H $*.h -d $*.d -o $*.t3x

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------