# Copyright (c) 2016 The Zcash developers
# Copyright (c) 2017 The BTCGPU developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

package=libsodium
$(package)_version=1.0.17
$(package)_download_path=https://download.libsodium.org/libsodium/releases
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=0CC3DAE33E642CC187B5CEB467E0AD0E1B51DCBA577DE1190E9FFA17766AC2B1
$(package)_dependencies=
$(package)_config_opts=
$(package)_config_opts_linux=--with-pic

define $(package)_preprocess_cmds
  cd $($(package)_build_subdir); ./autogen.sh
endef

define $(package)_config_cmds
  $($(package)_autoconf) --enable-static --disable-shared
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
