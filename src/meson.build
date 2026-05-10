project('plank','c')

cc = meson.get_compiler('c')

btrfs_dep = dependency('libbtrfsutil')
libmount_dep = dependency('mount')
libblkid_dep = dependency('blkid')

executable('plank', 'main.c', 'btrfs.c', 'commands.c', 'common.c', 'kernel.c', 'loader.c', 'plank.c',
  dependencies : [btrfs_dep, libmount_dep, libblkid_dep],
  install : true)
