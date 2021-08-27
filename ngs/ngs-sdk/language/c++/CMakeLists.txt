set( TARGET_NAME ngs-bind-c++ )

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

add_library( ${TARGET_NAME} ${SRC} )
