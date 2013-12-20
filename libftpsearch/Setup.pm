package FtpSearch::Setup;
require Exporter;

@ISA = ('Exporter');
@EXPORT = qw(
    $SEARCH_TIMEOUT $MAX_HITS $SEARCH_SERVER $BGCOLOR1 $BGCOLOR2 $PAGESIZE
    $CACHEROOT $CACHELIFE $DIRSIGN $EXTLINK $SYMLINKSIGN $SEARCHSIGN
    %sortindex %reqtype_index_search %reqtype_index_match
);

$SEARCH_TIMEOUT  = 15.0;  # search timeout, in seconds
$MAX_HITS        = 32768; # max. number of hits to retrieve
$SEARCH_SERVER   = 'sarth.sai.msu.ru';  # hostname of the FTP Search server
$BGCOLOR1        = '#C0C0C0';
$BGCOLOR2        = '#E0E0E0';
$PAGESIZE        = 25;

$CACHEROOT       = '/db1/w3/comps/discovery/db/ftpsearch/cache/';
$CACHELIFE       = 60*60; # time (in seconds) during which cache files
                          # are allowed to live

$EXTLINK         = '[&gt;&gt;]';
$DIRSIGN         = '[dir]';
$SYMLINKSIGN     = '--&gt;';
$SEARCHSIGN      = '[?]';

%sortindex = ('u', =>-1, 'n' => 0, 'p' => 1, 'h' => 2, 's' => 3, 't' => 4);
%reqtype_index_search = ('e' => 1, 's' => 2, 'x' => 3, 'g' => 4, 'n' => 8);
%reqtype_index_match  = ('e' => 1, 's' => 5, 'x' => 6, 'g' => 7, 'n' => 8);

1;

