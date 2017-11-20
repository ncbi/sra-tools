use strict;

while ( <> ) {
	$_ = "$1$2" if ( m|(^    <R.+=")/.+(/.+"/>\n$)|);
        print;
}
