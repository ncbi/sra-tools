set( SRC
	ReadCollection
	ReferenceSequence
	ReferenceIterator
	Reference
	PileupIterator
	Pileup
	PileupEventIterator
	PileupEvent
	AlignmentIterator
	Alignment
	ReadGroupIterator
	ReadGroup
	ReadIterator
	Read
	FragmentIterator
	Fragment
	Statistics
	StringRef
)

GenerateStaticLibs( ngs-bind-c++ "${SRC}" )
