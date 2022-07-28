use strict;

while ( <> ) {
    $_ = "$1$2" if ( m|(^    <R.+=")[h/].+(/.+"/>\n$)|);
    print;
}
