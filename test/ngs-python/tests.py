import unittest
from ngs import NGS
from ngs.ErrorMsg import ErrorMsg
from ngs.ReadCollection import ReadCollection
from ngs.ReferenceSequence import ReferenceSequence
from ngs.Alignment import Alignment
from ngs.Read import Read

PrimaryOnly           = "SRR1063272"
WithSecondary         = "SRR833251"
WithGroups            = "SRR822962"
WithCircularRef       = "SRR1769246"
SingleFragmentPerSpot = "SRR2096940";

def getRead(id):
    run = NGS.openReadCollection(PrimaryOnly)
    return run.getRead(id)

def getAlignment(id):
    run = NGS.openReadCollection(PrimaryOnly)
    return run.getAlignment(id)

def getSecondaryAlignment(id):
    run = NGS.openReadCollection(WithSecondary)
    return run.getAlignment(id)

def getReference():
    return NGS.openReadCollection(PrimaryOnly).getReference("supercont2.1")

def getReferenceSequence():
    return NGS.openReferenceSequence("NC_011752.1")

class Tests(unittest.TestCase):

    def fail(self):
        self.assertTrue(False)

    def test_open_success(self):
        run = NGS.openReadCollection(PrimaryOnly)

    def test_open_fail(self):
        try:
            run = NGS.openReadCollection("SRRsomejunk")
            self.fail()
        except ErrorMsg:
            pass

    def test_ReadCollection_getName(self):
        self.assertEqual(PrimaryOnly, NGS.openReadCollection(PrimaryOnly).getName())

    def test_ReadCollection_getReadGroup(self):
        gr = NGS.openReadCollection(PrimaryOnly).getReadGroup("C1ELY.6")

    def test_ReadCollection_getReadGroups(self):
        it = NGS.openReadCollection(PrimaryOnly).getReadGroups()

    def test_ReadCollection_getReferences(self):
        it = NGS.openReadCollection(PrimaryOnly).getReferences()

    def test_ReadCollection_getReference(self):
        ref = NGS.openReadCollection(PrimaryOnly).getReference("supercont2.1")

    def test_ReadCollection_hasReference(self):
        assert ( NGS.openReadCollection(PrimaryOnly).hasReference("supercont2.1") )
        assert ( not NGS.openReadCollection(PrimaryOnly).hasReference("non-existent acc") )

    def test_ReadCollection_getAlignment(self):
        al = NGS.openReadCollection(PrimaryOnly).getAlignment(PrimaryOnly + ".PA.1")

    def test_ReadCollection_getAlignments_Primary(self):
        alIt = NGS.openReadCollection(PrimaryOnly).getAlignments(Alignment.primaryAlignment)

    def test_ReadCollection_getAlignments_Secondary(self):
        alIt = NGS.openReadCollection(PrimaryOnly).getAlignments(Alignment.secondaryAlignment)

    def test_ReadCollection_getAlignments_all(self):
        alIt = NGS.openReadCollection(PrimaryOnly).getAlignments(Alignment.all)

    def test_ReadCollection_getAlignmentCount_PrimaryOnly(self):
        self.assertEqual(3987701, NGS.openReadCollection(PrimaryOnly).getAlignmentCount())

    def test_ReadCollection_getAlignmentCount_PrimaryOnly_Primary(self):
        self.assertEqual(3987701, NGS.openReadCollection(PrimaryOnly).getAlignmentCount(Alignment.primaryAlignment))

    def test_ReadCollection_getAlignmentCount_PrimaryOnly_Secondary(self):
        self.assertEqual(0, NGS.openReadCollection(PrimaryOnly).getAlignmentCount(Alignment.secondaryAlignment))

    def test_ReadCollection_getAlignmentCount_PrimaryOnly_All(self):
        self.assertEqual(3987701, NGS.openReadCollection(PrimaryOnly).getAlignmentCount(Alignment.all))

    def test_ReadCollection_getAlignmentCount_WithSecondary(self):
        self.assertEqual(178, NGS.openReadCollection(WithSecondary).getAlignmentCount())

    def test_ReadCollection_getAlignmentCount_WithSecondary_Primary(self):
        self.assertEqual(168, NGS.openReadCollection(WithSecondary).getAlignmentCount(Alignment.primaryAlignment))

    def test_ReadCollection_getAlignmentCount_WithSecondary_Secondary(self):
        self.assertEqual(10, NGS.openReadCollection(WithSecondary).getAlignmentCount(Alignment.secondaryAlignment))

    def test_ReadCollection_getAlignmentCount_WithSecondary_All(self):
        self.assertEqual(178, NGS.openReadCollection(WithSecondary).getAlignmentCount(Alignment.all))

    def test_ReadCollection_getAlignmentRange(self):
        # straddling primary and secondary alignments
        alIt = NGS.openReadCollection(WithSecondary).getAlignmentRange(166, 5)
        self.assertTrue(alIt.nextAlignment())
        self.assertEqual(WithSecondary + ".PA.166", alIt.getAlignmentId())

    def test_ReadCollection_getRead(self):
        read = NGS.openReadCollection(PrimaryOnly).getRead(PrimaryOnly + ".R.1")
        self.assertEqual(PrimaryOnly + ".R.1", read.getReadId())

    def test_ReadCollection_getReads(self):
        readIt = NGS.openReadCollection(PrimaryOnly).getReads(Read.all)
        self.assertTrue(readIt.nextRead())
        self.assertEqual(PrimaryOnly + ".R.1", readIt.getReadId())

    def test_ReadCollection_getReadCount(self):
        self.assertEqual(2280633, NGS.openReadCollection(PrimaryOnly).getReadCount())

    def test_ReadCollection_getReadRange(self):
        readIt = NGS.openReadCollection(PrimaryOnly).getReadRange(2, 3)
        self.assertTrue(readIt.nextRead())
        self.assertEqual(PrimaryOnly + ".R.2", readIt.getReadId())


# Read

    def test_Read_getReadCategory_full(self):
        self.assertEqual(Read.fullyAligned, getRead(PrimaryOnly + ".R.1").getReadCategory())

    def test_Read_getReadCategory_partial(self):
        self.assertEqual(Read.partiallyAligned, getRead(PrimaryOnly + ".R.3").getReadCategory())

    def test_Read_getNumFragments(self):
        self.assertEqual(2, getRead(PrimaryOnly + ".R.1").getNumFragments())

    def test_Read_fragmentIsAligned_partial(self):
        read = NGS.openReadCollection(PrimaryOnly).getRead(PrimaryOnly + ".R.3")
        self.assertEqual(True, read.fragmentIsAligned(0))
        self.assertEqual(False, read.fragmentIsAligned(1))

# FragmentIterator
    def test_FragmentIterator_ThrowsBeforeNext(self):
        try:
            getRead(PrimaryOnly + ".R.1").getFragmentId()
            self.fail()
        except ErrorMsg:
            pass

    def test_FragmentIterator_Next(self):
        read = getRead(PrimaryOnly + ".R.1")
        self.assertTrue(read.nextFragment())
        read.getReadCategory() # does not throw

# Fragment
    def test_Read_getFragmentId(self):
        read = getRead(PrimaryOnly + ".R.1")
        self.assertTrue(read.nextFragment())
        self.assertEqual(PrimaryOnly + ".FR0.1", read.getFragmentId())

    def test_getFragmentBases(self):
        read = getRead(PrimaryOnly + ".R.1")
        self.assertTrue(read.nextFragment())
        self.assertTrue(read.nextFragment())
        self.assertEqual("GGTA", read.getFragmentBases(2, 4));

    def test_getFragmentQualities(self):
        read = getRead(PrimaryOnly + ".R.1")
        self.assertTrue(read.nextFragment())
        self.assertTrue(read.nextFragment())
        self.assertEqual("@DDA", read.getFragmentQualities(2, 4))


# Alignment

    def test_Alignment_getAlignmentId(self):
        self.assertEqual(PrimaryOnly + ".PA.1", getAlignment(PrimaryOnly + ".PA.1").getAlignmentId())

    def test_Alignment_getReferenceSpec(self):
        self.assertEqual("supercont2.1", getAlignment(PrimaryOnly + ".PA.1").getReferenceSpec())

    def test_Alignment_getMappingQuality(self):
        self.assertEqual(60, getAlignment(PrimaryOnly + ".PA.1").getMappingQuality())

    def test_Alignment_getReferenceBases(self):
        self.assertEqual("ACTCGACATTCTGTCTTCGACCTATCTTTCTCCTCTCCCAGTCATCGCCCAGTAGAATTACCAGGCAATGAACCACGGCCTTTCATCCCAACGGCACAGCA",
                      getAlignment(PrimaryOnly + ".PA.1").getReferenceBases())

    def test_Alignment_getReadGroup(self):
        self.assertEqual("C1ELY.6", getAlignment(PrimaryOnly + ".PA.1").getReadGroup())

    def test_Alignment_getReadId(self):
        self.assertEqual(PrimaryOnly + ".R.165753", getAlignment(PrimaryOnly + ".PA.5").getReadId())

    def test_Alignment_getFragmentId(self):
        self.assertEqual(PrimaryOnly + ".FA0.1", getAlignment(PrimaryOnly + ".PA.1").getFragmentId())

    def test_Alignment_getFragmentBases_Raw(self):
        self.assertEqual("TGGATGCTCTGGAAAATCTGAAAAGTGGTGTTTGTAAGGTTTGCTGGCTGCCCATATACCACATGGATGATGGGGCTTTCCATTTTAATGTTGAAGGAGGA",
                      getAlignment(PrimaryOnly + ".PA.4").getFragmentBases())

    def test_Alignment_getFragmentQualities_Raw(self):
        self.assertEqual("######AA>55;5(;63;;3@;A9??;6..73CDCIDA>DCB>@B=;@B?;;ADAB<DD?1*>@C9:EC?2++A3+F4EEB<E>EEIEDC2?C:;AB+==1",
                      getAlignment(PrimaryOnly + ".PA.4").getFragmentQualities())

    def test_Alignment_getFragmentBases_Clipped(self):
        self.assertEqual("CTTCAACATTAAAATGGAAAGCCCCATCATCCATGTGGTATATGGGCAGCCAGCAAACCTTACAAACACCACTTTTCAGATTTTCCAGAGCATCCA",
                      getAlignment(PrimaryOnly + ".PA.4").getClippedFragmentBases())

    def test_Alignment_getFragmentQualities_Clipped(self):
        self.assertEqual("#AA>55;5(;63;;3@;A9??;6..73CDCIDA>DCB>@B=;@B?;;ADAB<DD?1*>@C9:EC?2++A3+F4EEB<E>EEIEDC2?C:;AB+==1",
                      getAlignment(PrimaryOnly + ".PA.4").getClippedFragmentQualities())

    def test_Alignment_getAlignedFragmentBases(self):
        self.assertEqual("ATATGGGTTCACTCCAACAGTGAACCATTCCAAAAGACCTTGCCTGCGTGGCCATCTCCTCACAAACCCACCATCCCGCAACATCTCAGGTATCATACCTT",
                      getAlignment(PrimaryOnly + ".PA.2").getAlignedFragmentBases())

    def test_Alignment_getAlignmentCategory(self):
        self.assertEqual(Alignment.primaryAlignment, getAlignment(PrimaryOnly + ".PA.4").getAlignmentCategory())

    def test_Alignment_getAlignmentPosition(self):
        self.assertEqual(85, getAlignment(PrimaryOnly + ".PA.1").getAlignmentPosition())

    def test_Alignment_getAlignmentLength(self):
        self.assertEqual(101, getAlignment(PrimaryOnly + ".PA.1").getAlignmentLength())

    def test_Alignment_getIsReversedOrientation_False(self):
        self.assertFalse(getAlignment(PrimaryOnly + ".PA.1").getIsReversedOrientation())

    def test_Alignment_getIsReversedOrientation_True(self):
        self.assertTrue(getAlignment(PrimaryOnly + ".PA.2").getIsReversedOrientation())

    def test_Alignment_getSoftClip_None(self):
        al = getAlignment(PrimaryOnly + ".PA.1")
        self.assertEqual(0, al.getSoftClip(Alignment.clipLeft))
        self.assertEqual(0, al.getSoftClip(Alignment.clipRight))

    def test_Alignment_getSoftClip_Left(self):
        al = getAlignment(PrimaryOnly + ".PA.4")
        self.assertEqual(5, al.getSoftClip(Alignment.clipLeft))
        self.assertEqual(0, al.getSoftClip(Alignment.clipRight))

    def test_Alignment_getSoftClip_Right(self):
        al = getAlignment(PrimaryOnly + ".PA.10")
        self.assertEqual(0,  al.getSoftClip(Alignment.clipLeft))
        self.assertEqual(13, al.getSoftClip(Alignment.clipRight))

    def test_Alignment_getTemplateLength(self):
        self.assertEqual(201, getAlignment(PrimaryOnly + ".PA.1").getTemplateLength())

    def test_Alignment_getShortCigar_Unclipped(self):
        self.assertEqual("5S96M", getAlignment(PrimaryOnly + ".PA.4").getShortCigar(False))

    def test_Alignment_getShortCigar_Clipped(self):
        self.assertEqual("96M", getAlignment(PrimaryOnly + ".PA.4").getShortCigar(True))

    def test_Alignment_getLongCigar_Unclipped(self):
        self.assertEqual("5S1X8=1X39=1X46=", getAlignment(PrimaryOnly + ".PA.4").getLongCigar(False))

    def test_Alignment_getLongCigar_Clipped(self):
        self.assertEqual("1X8=1X39=1X46=", getAlignment(PrimaryOnly + ".PA.4").getLongCigar(True))

    def test_Alignment_hasMate_Primary_No(self):
        self.assertFalse(getAlignment(PrimaryOnly + ".PA.99").hasMate())

    def test_Alignment_hasMate_Primary_Yes(self):
        self.assertTrue(getAlignment(PrimaryOnly + ".PA.1").hasMate())

    def test_Alignment_hasMate_Secondary(self):
        self.assertFalse(getSecondaryAlignment(WithSecondary + ".SA.169").hasMate())

    def test_Alignment_getMateAlignmentId(self):
        self.assertEqual(PrimaryOnly + ".PA.2", getAlignment(PrimaryOnly + ".PA.1").getMateAlignmentId())

    def test_Alignment_getMateAlignmentId_Missing(self):
        try:
            getAlignment(PrimaryOnly + ".PA.99").getMateAlignmentId()
            self.fail()
        except ErrorMsg:
            pass

    def test_Alignment_getMateAlignmentId_SecondaryThrows(self):
        try:
            getSecondaryAlignment(WithSecondary + ".SA.172").getMateAlignmentId()
            self.fail()
        except ErrorMsg:
            pass

    def test_Alignment_getMateAlignment(self):
        self.assertEqual(PrimaryOnly + ".PA.2", getAlignment(PrimaryOnly + ".PA.1").getMateAlignment().getAlignmentId())

    def test_Alignment_getMateAlignment_Missing(self):
        try:
            getAlignment(PrimaryOnly + ".PA.99").getMateAlignment()
            self.fail()
        except ErrorMsg:
            pass

    def test_Alignment_getMateAlignment_SecondaryThrows(self):
        try:
            getSecondaryAlignment(WithSecondary +".SA.172").getMateAlignment ()
            self.fail()
        except ErrorMsg:
            pass

    def test_Alignment_getMateReferenceSpec(self):
        self.assertEqual("supercont2.1",  getAlignment(PrimaryOnly + ".PA.1").getMateReferenceSpec())

    def test_Alignment_getMateIsReversedOrientation_Yes(self):
        self.assertTrue(getAlignment(PrimaryOnly + ".PA.1").getMateIsReversedOrientation())

    def test_Alignment_getMateIsReversedOrientation_No(self):
        self.assertFalse(getAlignment(PrimaryOnly + ".PA.2").getMateIsReversedOrientation())

    def test_Alignment_isPaired_MultiFragmentsPerSpot(self):
        readCollection = NGS.openReadCollection(PrimaryOnly)
        alignment = readCollection.getAlignment(PrimaryOnly + ".PA.1")
        self.assertTrue(alignment.isPaired())

        alignment = readCollection.getAlignment(PrimaryOnly + ".PA.2")
        self.assertTrue(alignment.isPaired())

        # has unaligned mate
        alignment = readCollection.getAlignment (PrimaryOnly + ".PA.6")
        self.assertTrue(alignment.isPaired())

    def test_Alignment_isPaired_SingleFragmentPerSpot(self):
        readCollection = NGS.openReadCollection(SingleFragmentPerSpot)
        alignment = readCollection.getAlignment(SingleFragmentPerSpot + ".PA.1")
        self.assertFalse(alignment.isPaired())

# ReferenceSequence
    def test_ReferenceSequence_getCanonicalName(self):
        self.assertEqual("gi|218511148|ref|NC_011752.1|", getReferenceSequence().getCanonicalName())

    def test_ReferenceSequence_getIsCircular_Yes(self):
        self.assertTrue(getReferenceSequence().getIsCircular())

    def test_ReferenceSequence_getLength(self):
        self.assertEqual(72482, getReferenceSequence().getLength())

    def test_ReferenceSequence_getReferenceBases(self):
        self.assertEqual("ATAAA", getReferenceSequence().getReferenceBases(72482 - 5))

    def test_ReferenceSequence_getReferenceBases_Length(self):
        self.assertEqual("TACA", getReferenceSequence().getReferenceBases(4998, 4))

    def test_ReferenceSequence_getReferenceChunk(self):
        self.assertEqual("TAATA", getReferenceSequence().getReferenceChunk(5000 - 5, 5))

    def test_ReferenceSequence_getReferenceChunk_Length (self):
        self.assertEqual("TAATA", getReferenceSequence().getReferenceChunk(5000 - 5, 10))

# Reference
    def test_Reference_getCommonName(self):
        self.assertEqual("supercont2.1", getReference().getCommonName())

    def test_Reference_getCanonicalName(self):
        self.assertEqual("NC_000007.13", NGS.openReadCollection("SRR821492").getReference("chr7").getCanonicalName())

    def test_Reference_getIsCircular_No(self):
        self.assertFalse(getReference().getIsCircular())

    def test_Reference_getIsCircular_Yes(self):
        self.assertTrue(NGS.openReadCollection("SRR821492").getReference("chrM").getIsCircular())

    def test_Reference_getIsLocal_Yes(self):
        self.assertTrue(getReference().getIsLocal())

    def test_Reference_getIsLocal_No(self):
        self.assertFalse(NGS.openReadCollection("SRR821492").getReference("chrM").getIsLocal())

    def test_Reference_getLength(self):
        self.assertEqual(2291499, getReference().getLength())

    def test_Reference_getReferenceBases(self):
        self.assertEqual("ATCTG", getReference().getReferenceBases(2291499 - 5))

    def test_Reference_getReferenceBases_Length(self):
        self.assertEqual("GCGCTATGAC", getReference().getReferenceBases(9000, 10))

    def test_Reference_getReferenceChunk(self):
        self.assertEqual("CTAGG", getReference().getReferenceChunk(5000 - 5, 5))

    def test_Reference_getReferenceChunk_Length (self):
        self.assertEqual("GCGCTATGAC", getReference().getReferenceChunk(9000, 10))

    def test_Reference_getAlignment(self):
        self.assertEqual(PrimaryOnly + ".PA.1", getReference().getAlignment(PrimaryOnly + ".PA.1").getAlignmentId())

#TODO: getAlignmentCount
#TODO: getAlignmentCount_Filtered

#TODO: getPileups
#TODO: getPileupRange
#TODO: getPileupRange_Filtered

# ReferenceIterator

    def test_ReferenceIterator_ThrowBeforeNext(self):
        it = NGS.openReadCollection(PrimaryOnly).getReferences()
        try:
            it.getCommonName()
            self.fail()
        except ErrorMsg:
            pass

    def test_ReferenceIterator_Next(self):
        it = NGS.openReadCollection(PrimaryOnly).getReferences()
        self.assertTrue(it.nextReference())
        self.assertEqual("supercont2.1", it.getCommonName())

# AlignmentIterator from Reference (ReferenceWindow)
    def test_ReferenceWindow(self):
        it = NGS.openReadCollection(WithSecondary).getReference("gi|169794206|ref|NC_010410.1|").getAlignments(Alignment.all)
        self.assertTrue(it.nextAlignment())

        # the first 2 secondary alignments' locations on the list: #34, #61
        count = 1;
        while it.nextAlignment():
            if it.getAlignmentCategory() == Alignment.secondaryAlignment:
                break
            count += 1

        self.assertEqual(34, count)
        while it.nextAlignment():
            if it.getAlignmentCategory() == Alignment.secondaryAlignment:
                break
            count += 1

        self.assertEqual(61, count)

    def test_ReferenceWindow_Slice(self):
        it = NGS.openReadCollection(WithSecondary).getReference("gi|169794206|ref|NC_010410.1|").getAlignmentSlice(516000, 100000)
        self.assertTrue(it.nextAlignment())
        self.assertEqual(WithSecondary + ".PA.33", it.getAlignmentId())
        self.assertTrue(it.nextAlignment())
        self.assertEqual(WithSecondary + ".PA.34", it.getAlignmentId())
        self.assertTrue(it.nextAlignment())
        self.assertEqual(WithSecondary + ".SA.169", it.getAlignmentId()) #secondary
        self.assertTrue(it.nextAlignment())
        self.assertEqual(WithSecondary + ".PA.35", it.getAlignmentId())
        self.assertFalse(it.nextAlignment())

    def test_ReferenceWindow_Slice_Filtered_Category (self):
        it = NGS.openReadCollection(WithSecondary).getReference("gi|169794206|ref|NC_010410.1|").getAlignmentSlice(516000, 100000, Alignment.primaryAlignment)
        self.assertTrue(it.nextAlignment())
        self.assertEqual(WithSecondary + ".PA.33", it. getAlignmentId())
        self.assertTrue(it.nextAlignment())
        self.assertEqual(WithSecondary + ".PA.34", it. getAlignmentId())
        self.assertTrue(it.nextAlignment())
        self.assertEqual(WithSecondary + ".PA.35", it. getAlignmentId()) # no secondary
        self.assertFalse(it.nextAlignment())

    def test_ReferenceWindow_Slice_Filtered_Start_Within_Slice (self):
        ref = NGS.openReadCollection(WithCircularRef).getReference("NC_012920.1")
        it = ref.getFilteredAlignmentSlice(0, ref.getLength(), Alignment.all, Alignment.startWithinSlice, 0)

        self.assertTrue(it.nextAlignment())
        lastAlignmentPosition = it.getAlignmentPosition()
        while it.nextAlignment():
            currentPosition = it.getAlignmentPosition()
            errorMsg = "Sorting violated. Last position (" + str(lastAlignmentPosition) + ") is higher than current one (" + str(currentPosition) + ")"
            self.assertTrue ( lastAlignmentPosition <= currentPosition, errorMsg )
            lastAlignmentPosition = currentPosition

    # ReadGroup
    def test_ReadGroup_getName(self):
        gr = NGS.openReadCollection(PrimaryOnly).getReadGroup("C1ELY.6")
        self.assertEqual("C1ELY.6", gr.getName())

    def test_ReadGroup_has(self):
        assert ( NGS.openReadCollection(PrimaryOnly).hasReadGroup("C1ELY.6") )
        assert ( not NGS.openReadCollection(PrimaryOnly).hasReadGroup("non-existent read group") )

    def test_ReadGroup_getStatistics(self):
        gr = NGS.openReadCollection(WithGroups).getReadGroup("GS57510-FS3-L03")

        stats = gr.getStatistics()

        self.assertEqual(34164461870, stats.getAsU64("BASE_COUNT"))
        self.assertEqual(34164461870, stats.getAsU64("BIO_BASE_COUNT"))
        self.assertEqual(488063741,   stats.getAsU64("SPOT_COUNT"))
        self.assertEqual(5368875807,  stats.getAsU64("SPOT_MAX"))
        self.assertEqual(4880812067,  stats.getAsU64("SPOT_MIN"))

    # def test_ReadGroup_getRead(self):
        # gr = NGS.openReadCollection(PrimaryOnly).getReadGroup("C1ELY.6")
        # r = gr.getRead(PrimaryOnly + ".R.1")
        # self.assertEqual("C1ELY.6", r.getReadGroup())

    # def test_ReadGroup_getReads(self):
        # gr = NGS.openReadCollection(PrimaryOnly).getReadGroup("C1ELY.6")
        # r = gr.getReads(Read.partiallyAligned)

    # ReadGroupIterator
    def test_ReadGroupIterator_ThrowBeforeNext(self):
        it = NGS.openReadCollection(PrimaryOnly).getReadGroups()
        try:
            it.getName()
            self.fail()
        except ErrorMsg:
            pass

    def test_ReadGroupIterator_Next(self):
        it = NGS.openReadCollection(PrimaryOnly).getReadGroups();
        self.assertTrue(it.nextReadGroup());
        name = it.getName();

if __name__ == "__main__":
    unittest.main()
