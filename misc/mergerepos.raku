# Script that was used to convert enosi's projects from submodules to actual
# trees in the enosi project. This handles merging and properly ordering the
# commit history. 
#
# One downside is that merge commits are removed, but this isn't a huge deal
# for us because we never really used branches and keeping conflict merge 
# commits around isnt that important.
#
# Keeping around in case we ever need to something like this again and need to
# remember how to do some part of this.

run <rm -rfd enosi>;
run <git clone https://github.com/agudbrand/enosi enosi>;

chdir 'enosi';

run <git config set sequence.editor ../seqedit.raku>;

run <git checkout -b main_fixed>;

my @repos = <iro lake lpp lppclang ecs hreload>;

run (("git filter-repo --force --invert-paths " ~ ("--path " <<~<< @repos)).words);

spurt "../seqedit.raku", q:to/END/;
  #!/bin/raku
  sub MAIN($path)
  {
    my $out = "";
    for $path.IO.lines -> $line
    {
      if $line ~~ /^pick(.*)/ 
      {
        $out ~= "edit $0\n";
      }
    }
    spurt $path, $out;
  }
  END

chmod 0o740, '../seqedit.raku';

run <git rebase -i --root>;

run <rm .gitmodules>;

run <git add .>;

run <git commit --amend --no-edit>;

run <git rebase --continue>;

my $repos = @repos.join('||');

loop
{
  my $result = run(<git rebase --continue>,:out,:err);

  last if $result.err.slurp ~~ /"fatal:"/;

  my $out = "";
  my $gm = ".gitmodules".IO;

  if $gm.e
  {
    my @lines = gather 
    {
      my $skip_scmarkers = False;
      my $skip_badrepo = False;
      for $gm.lines -> $line
      {
        my $badreporx = /\[submodule\s\"(<$repos>)\"\]/;

        next if $line ~~ /">>>>>>>"/;

        if $skip_scmarkers
        {
          if $line ~~ /"======="/
          {
            $skip_scmarkers = False;
          }
          next;
        }
        else
        {
          if $line ~~ /"<<<<<<<"/ 
          {
            $skip_scmarkers = True;
            next;
          }
        }

        if $skip_badrepo 
        {
          if $line ~~ /\[/ and $line !~~ $badreporx
          {
            $skip_badrepo = False;
          }
          else
          {
            next;
          }
        }
        elsif $line ~~ $badreporx
        {
          $skip_badrepo = True;
          next;
        }

        take $line;
      }
    }

    spurt ".gitmodules", @lines.join("\n");
    run <git add .>;
    run <git commit --amend --no-edit>;
  }
}


for @repos -> $repo
{
  run <<git remote add $repo "https://github.com/agudbrand/$repo">>;
  run <<git fetch $repo>>;
  run <<git checkout "$repo/main">>;
  run <<git checkout -b "{$repo}_fix">>;
  run <<git filter-repo --path-rename ":{$repo}/" --partial --force --refs "{$repo}_fix">>;

  spurt "../expr.txt", qq:to/END/;
    regex:^(.+)==>$repo: \\1
    END

  run <<git filter-repo --refs "{$repo}_fix" --replace-message ../expr.txt>>;
}

run <<git checkout main_fixed>>;
run <<git checkout -b main_rebased>>;

for @repos -> $repo
{
  run <<git rebase "{$repo}_fix" -X ours>>;
}

my $log = run(<<git log "--pretty='%H %at'" --no-merges>>,:out).out.slurp;

my $sorter = run(<<sort -k2 -n>>, :in, :out);

$sorter.in.say($log);
$sorter.in.close;

my $sorted = "";

for $sorter.out.lines -> $line
{
  my $hash = $line.match(/^\'(<.xdigit>+)/)[0];
  next unless $hash;
  $sorted ~= "pick " ~ $hash ~ "\n";
}

spurt "../seqedit.raku", q:to/END/;
  #!/bin/raku
  sub MAIN($path)
  {
    spurt $path, qq:to/STOP/;
    \qq[$sorted]
    STOP
  }
  END

run <<git rebase -i --root>>;

for @repos -> $repo
{
  run <<git branch -D "{$repo}_fix">>;
  run <<git remote remove {$repo}>>;
}

run <<git branch -D main_fixed>>;
run <<git branch -D main>>;
run <<git checkout -b main>>;
run <<git branch -D main_rebased>>;

run <<git filter-repo --commit-callback "
      commit.committer_name = commit.author_name
      commit.committer_email = commit.author_email
      commit.committer_date = commit.author_date">>;

