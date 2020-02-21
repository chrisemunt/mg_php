dnl config.m4 for extension mg_php

PHP_ARG_ENABLE(mg_php, whether to enable mg_php support,
[  --disable-mg_php          Disable mg_php support], yes)

if test "$PHP_MG_PHP" != "no"; then
  AC_DEFINE([HAVE_MG_PHP],1 ,[whether to enable mg_php support])
  AC_HEADER_STDC

PHP_NEW_EXTENSION(mg_php,
	  mg_php.c \
	  mg_dba.c,
	  $ext_shared)
  PHP_INSTALL_HEADERS([ext/mg_php], [php_mg_php.h mg_dba.h mg_dbasys.h])
  PHP_ADD_MAKEFILE_FRAGMENT()
  PHP_SUBST(MG_PHP_SHARED_LIBADD)
fi
