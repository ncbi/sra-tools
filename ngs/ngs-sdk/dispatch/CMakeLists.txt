set( SRC
	ReadCollectionItf
	ReferenceItf
	ReferenceSequenceItf
	PileupItf
	PileupEventItf
	AlignmentItf
	ReadGroupItf
	ReadItf
	FragmentItf
	StatisticsItf
	PackageItf
	StringItf
	Refcount
	VTable
	ErrBlock
	ErrorMsg
)

GenerateStaticLibs( ngs-disp "${SRC}" )