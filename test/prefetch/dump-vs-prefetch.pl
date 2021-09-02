#print `vdb-config -on repository`; die if $?;
`vdb-dump -CREAD -R1 SRR619505 -+VFS`; die if $?;
`prefetch            SRR619505 -+VFS`; die if $?;
