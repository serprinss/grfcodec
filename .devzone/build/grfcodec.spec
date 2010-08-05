Name:           %{dz_repo}
Version:        0.9.10+r2294+%{dz_version}
Release:        %{_vendor}%{?suse_version}
Summary:        A suite of programs to modify Transport Tycoon Deluxe's GRF files
Group:          Development/Tools
License:        GPLv2+
URL:            http://dev.openttdcoop.org/projects/grfcodec/
Source0:        grfcodec-%{dz_version}.tar

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-buildroot

BuildRequires:  gcc-c++
BuildRequires:  boost-devel
#We need Mercurial for auto version detection:
BuildRequires:  mercurial

%description
A suite of programs to modify Transport Tycoon Deluxe's GRF files.
This program is needed to de-/encode graphic extenions, which you
need to build OpenGFX.

%prep
%setup -qn %{name}

%build
make %{?_smp_mflags} UPX= release bundle_src

%install
make install INSTALLPATH=%{buildroot}%{_bindir}

%clean

%files
%defattr(-,root,root,-)
%doc Changelog COPYING grfcodec.txt grftut.txt grf.txt
%{_bindir}/grfcodec
%{_bindir}/grfdiff
%{_bindir}/grfid
%{_bindir}/grfmerge

%changelog