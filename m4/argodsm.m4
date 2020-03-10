AC_DEFUN([AC_CHECK_ARGODSM],
	[
		AC_ARG_WITH(
			[argodsm],
			[AS_HELP_STRING([--with-argodsm=prefix], [specify the installation prefix of ArgoDSM])],
			[ ac_cv_use_argodsm_prefix=$withval ],
			[ ac_cv_use_argodsm_prefix="" ]
		)
		
		if test x"${ac_cv_use_argodsm_prefix}" != x"" ; then
			AC_MSG_CHECKING([the ArgoDSM installation prefix])
			AC_MSG_RESULT([${ac_cv_use_argodsm_prefix}])
			argodsm_LIBS="-Wl,-rpath,${ac_cv_use_argodsm_prefix}/lib -L${ac_cv_use_argodsm_prefix}/lib -largo -largobackend-mpi"
			argodsm_CPPFLAGS="-I${ac_cv_use_argodsm_prefix}/include"
			ac_use_argodsm=yes
		fi
		
		AM_CONDITIONAL(HAVE_ARGODSM, test x"${ac_use_argodsm}" = x"yes")
		AM_CONDITIONAL(USE_ARGODSM, test x"${ac_use_argodsm}" = x"yes")

		if test x"{ac_use_argodsm}" = x"yes" ; then
			AC_DEFINE([USE_ARGODSM], [1], [Define if ArgoDSM is enabled.])
		fi
		
		AC_SUBST([argodsm_LIBS])
		AC_SUBST([argodsm_CPPFLAGS])
	]
)
