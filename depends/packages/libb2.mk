# Copyright (c) 2016 The Zcash developers
# Copyright (c) 2017 The BTCGPU developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

package=libb2
$(package)_version=0.98
$(package)_download_path=https://github.com/BLAKE2/libb2/releases/download/v$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=e869e0c3a93bc56d1052eccbe9cd925b8a8c7308b417532829a700cf374b036f
$(package)_patches=cross_compile.patch
$(package)_build_opts+=CFLAGS="$($(package)_cflags) $($(package)_cppflags) -fPIC"

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --enable-static --enable-native=no
endef

define $(package)_preprocess_cmds
  patch ./configure < $($(package)_patch_dir)/cross_compile.patch
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE) $($(package)_build_opts)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef