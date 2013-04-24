cd jboss-as
git bisect reset
git bisect start
git bisect good 0e3e092eb97d2b3d324ca623db0dabc949a6fbbe
git bisect bad f7a03d77c5f52fd302041cfa686671c7acf37a34
git show | head
git blame pom.xml | head
git bisect bad
git show | head
git blame pom.xml | head
git bisect good
git show | head
git blame pom.xml | head
git bisect bad
git show | head
git blame pom.xml | head
git bisect good
git show | head
git blame pom.xml | head
git bisect bad
git show | head
git blame pom.xml | head
git bisect good
git show | head
git blame pom.xml | head
git bisect bad
git show | head
git blame pom.xml | head
git bisect good
git show | head
git blame pom.xml | head
git bisect bad
git show | head
git blame pom.xml | head
git bisect good
git show | head
git blame pom.xml | head
git bisect bad
git show | head
git blame pom.xml | head
git bisect good
git show | head
git bisect reset
