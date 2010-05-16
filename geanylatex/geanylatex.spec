%global svnrev 1003

# $Revision:$, $Date:$
Summary:	Geany LaTeX plugin
Summary(pl.UTF-8): wtyczka Geany dla LaTeXa
Summary(de.UTF-8): LaTex Plugin für Geany
Name:		geany-plugin-latex

Version:	0.5
Release:	1
License:	GPLv2+

# rpmlint: geanylatex.spec:9: W: non-standard-group Libraries
Group:		Libraries
Source0:	http://frank.uvena.de/files/geany/testing/geanylatex-%{version}dev-svn%{?svnrev:%{svnrev}}.tar.bz2
# Source0-md5:	f0f3b602d4d9cbe7c659f852abd74f29
URL:		http://frank.uvena.de/en/Geany/geanylatex/
BuildRequires:	geany-devel >= 0.18

# gettext-devel -> /usr/bin/msgfmt
BuildRequires:	gettext-devel

#BuildRequires:	gtk+2-devel >= 2:2.8
BuildRequires:	gtk2-devel >= 2.8

BuildRequires:	intltool
BuildRequires:	pkgconfig

#BuildRequires:	rpmbuild(macros) >= 1.198

BuildRequires:	waf
Requires:	geany >= 0.19

# Requires:	tetex
BuildRoot:	%{tmpdir}/%{name}-%{version}-root-%(id -u -n)

%description
Geany LaTeX is a little plugin to improve support of LaTeX on Geany.
It implements a couple of mayby useful functions:
    - Wizard to create new LaTeX documents in a fast and easy way with a
      bunch of templates available
    - A front end for add labels \label and references \ref and \pageref
      with getting suggestion from aux file of document
    - Inserting special characters through menu
    - Help entering the right fields for BibTeX entries by providing
      templates
    - Easy inserting format patterns like \texttt through menu
    - Support on inserting environments by offering an dialog and
      recognising selections
    - Shortcuts for inserting \item and \newline
    - Toolbar with often used format options
    - Adding some autocompletion feature as e.g. autoclosing of
      environments and adding {} in some cases where it might be useful

%description -l pl.UTF-8
Geany LaTeX jest małą wtyczką, która udostępnia wsparcie LaTeXa w
Geany. Implementuje wiele być może przydatnych funkcji:
    - czarodziej pozwala stworzyć nowy dokument LaTeXa szybko i w łatwy
      sposób, dzięki wielu dostępnym szablonom
    - interfejs użytkownika dodaje etykiety \label i referencje \ref oraz
      \pageref, dzięki sugestiom z pliku aux
    - wstawianie znaków specjalnych z menu
    - pomaga poprawnie wypełniać pola BibTeX dzięki szablonom
    - proste wstawianie formatowania, jak na przykład \texttt, przez menu
    - wsparcie środowiska przy pisaniu dzięki wyświetlaniu opcji wyboru
      oraz jego zatwierdzeniu
    - skróty dla wstawiania \item i \newline
    - pasek narzędzi z często używanymi opcjami formatowania

%description -l de.UTF-8
GeanyLaTeX ist ein Plugin für Geany, das bei der Arbeit mit LaTeX-Dateien
helfen soll.
Dabei implementiert es eine Reihe von nützlichen Funktionen
	- Dialog zum einfachen Erstellen von typischen Dokumenten
    - Unterstützung beim Einfügen von \label und Referenzen wie \ref und
      \pageref
    - Hilfe beim Einfügen und Ersetzen von Sonderzeichen
    - Einfaches Einfügen von Formtierungen wie \texttt über Menü bzw.
      Tastendruck
    - Einfügen von Umgebung über Das Menü bzw. Tastendruck
    - Werkzeugleiste mit oft genutzten Formatierungen
    - Autvervollständigungen bei z.B. \begin{} das schließende \end{}
    - Automatisches Hinzufügen von {} an möglicherweiße sinnvollen Stellen

%clean
rm -rf $RPM_BUILD_ROOT

%prep
%setup -q -n geanylatex-%{version}dev-svn%{?svnrev:%{svnrev}}

%build

waf configure \
	--prefix=%{_prefix}

%install
rm -rf $RPM_BUILD_ROOT

waf install \
	--destdir $RPM_BUILD_ROOT

# install statt mv benutzen, und timestamps beibehalten
install -dp $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}/img
install -p $RPM_BUILD_ROOT%{_docdir}/geany-plugins/geanylatex/geany* $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}/
install -p $RPM_BUILD_ROOT%{_docdir}/geany-plugins/geanylatex/img/*  $RPM_BUILD_ROOT%{_docdir}/%{name}-%{version}/img/

%find_lang geanylatex

%files -f geanylatex.lang
%defattr(644,root,root,755)
%{_libdir}/geany/geanylatex.so

%doc /usr/share/doc/geany-plugins/geanylatex/*
%doc %{_docdir}/%{name}-%{version}/geanylatex*
%doc %{_docdir}/%{name}-%{version}/img/*.png

# rpmlint: geanylatex.spec:103: W: macro-in-%changelog %{date}

%define date	%(echo `LC_ALL="C" date +"%a %b %d %Y"`)
%changelog
* Sat May 15 2010 Frank Lanitz <frank@đrank.uvena.de>
- Some minor update of spec

* Wed Jan 27 2010 Dominic Hopf <dmaphy@fedoraproject.org>
- use %%global svnrev
- preserve timestamps of installed documentation
- correct some package names, so they fit for Fedora
- use system-wide installed waf instead of non-existent macro %%waf
- correct the %%files listing, to avoid possible error messages

* Wed Jan 27 2010 Frank Lanitz <frank@frank.uvena.de>
- Updated version of this specfile

* Sat Nov 7 2009 Krzysztof Goliński <krzysztof.golinski@gmail.com>
- Initial version of this specfile for PLD Linux

$Log:$
