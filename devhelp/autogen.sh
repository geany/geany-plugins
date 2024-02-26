#!/bin/sh

autoreconf -vfi		|| exit 1

echo 'Build system setup OK.'
echo 'Now type `./configure` to configure the package.'
