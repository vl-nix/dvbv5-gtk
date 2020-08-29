config_h = configuration_data()

set_defines = [
  ['ICONV_CONST', ''],
  ['ENABLE_NLS', 1],
  ['HAVE_BACKTRACE', 1],
  ['HAVE_DVBV5_REMOTE', 1],
  ['HAVE_PTHREAD', 1],
  ['HAVE_SECURE_GETENV', 1],
  ['LIBDVBV5_DOMAIN', '"libdvbv5"'],
  ['LOCALEDIR', '"/usr/share/locale"'],
  ['V4L_UTILS_VERSION', '"1.20.0"']
]

foreach define: set_defines
  config_h.set(define[0], define[1])
endforeach

configure_file(output: 'config.h', configuration: config_h)
