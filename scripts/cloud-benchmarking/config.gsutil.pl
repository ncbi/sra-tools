{
    SDL_LOCATION => 'gs',
    NATIVE_TOOL_NAME => 'gsutil',
    NATIVE_TOOL_CMD => 'gsutil cp',
    NATIVE_TOOL_COPY_CMD => sub {
        my @y = ( 'gsutil' );
        push @y, '-q' unless ($_[1] // 0);
        push @y, 'cp', $_[0]->{'url'}, './';
        @y
    }
}
