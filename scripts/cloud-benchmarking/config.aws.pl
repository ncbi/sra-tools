{
    SDL_LOCATION => 's3',
    NATIVE_TOOL_NAME => 'aws',
    NATIVE_TOOL_CMD => 'aws s3 cp',
    NATIVE_TOOL_COPY_CMD => sub {
        my @y = qw[ aws s3 cp --no-sign-request ];
        push @y, '--source-region', $_[0]->{'region'} if $_[0]->{'region'};
        push @y, '--quiet' unless ($_[1] // 0);
        push @y, '--dryrun' if $_[2];
        push @y, $_[0]->{'url'}, './';
        @y
    }
}
