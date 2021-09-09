set( SRC
	ReadCollectionItf
	PileupItf
	PileupEventItf
	ReferenceItf
	ReferenceSequenceItf
	AlignmentItf
	ReadGroupItf
	ReadItf
	FragmentItf
	StatisticsItf
	StringItf
	Refcount
	ErrorMsg
	ErrBlock
)

GenerateStaticLibs( ngs-adapt-c++ "${SRC}" )