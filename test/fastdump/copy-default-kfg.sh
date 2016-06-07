# make sure we can find remote repository if site repository does not exist

${BINDIR}/vdb-config -on repository/site/main/tracearc/root
if [ "$?" != 0 ] ; then
    if [ -f   ${BINDIR}/../../../../../../../ncbi-vdb/libs/kfg/default.kfg ]
    then
        cp -v ${BINDIR}/../../../../../../../ncbi-vdb/libs/kfg/default.kfg \
                                                    ${BINDIR}/ncbi
    else
        cp -v ${VDB_INCDIR}/../libs/kfg/default.kfg ${BINDIR}/ncbi
    fi
fi
