# Define the kmod package name here.
%define kmod_name led-ugreen

# If kmod_kernel_version isn't defined on the rpmbuild line, define it here.
%{!?kmod_kernel_version: %define kmod_kernel_version 5.14.0-427.20.1.el9_4}

%{!?dist: %define dist .el9}

Name:           kmod-%{kmod_name}
Version:        0.1
Release:        15%{?dist}
Summary:        %{kmod_name} kernel module(s)
Group:          System Environment/Kernel
License:        GPLv2
URL:            https://github.com/miskcoo/ugreen_leds_controller

# Sources.
#tar -czf ugreen_leds_controller.tar.gz ugreen_leds_controller
Source0:        %{kmod_name}-%{version}.tar.gz

# Fix for the SB-signing issue caused by a bug in /usr/lib/rpm/brp-strip
# https://bugzilla.redhat.com/show_bug.cgi?id=1967291

%define __spec_install_post \
	/usr/lib/rpm/check-buildroot \
	/usr/lib/rpm/redhat/brp-ldconfig \
	/usr/lib/rpm/brp-compress \
	/usr/lib/rpm/brp-strip-comment-note /usr/bin/strip /usr/bin/objdump \
	/usr/lib/rpm/brp-strip-static-archive /usr/bin/strip \
	/usr/lib/rpm/brp-python-bytecompile "" "1" "0" \
	/usr/lib/rpm/brp-python-hardlink \
	/usr/lib/rpm/redhat/brp-mangle-shebangs

# Source code patches

%define findpat %( echo "%""P" )
%define dup_state_dir %{_localstatedir}/lib/rpm-state/kmod-dups
%define kver_state_dir %{dup_state_dir}/kver
%define kver_state_file %{kver_state_dir}/%{kmod_kernel_version}.%{_arch}
%define dup_module_list %{dup_state_dir}/rpm-kmod-%{kmod_name}-modules
%define debug_package %{nil}

%global _use_internal_dependency_generator 0
%global kernel_source() %{_usrsrc}/kernels/%{kmod_kernel_version}.%{_arch}

BuildRoot:      %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)

ExclusiveArch:  x86_64

BuildRequires:  elfutils-libelf-devel
BuildRequires:  kernel-devel = %{kmod_kernel_version}
BuildRequires:  kernel-abi-stablelists
BuildRequires:  kernel-rpm-macros
BuildRequires:  redhat-rpm-config
BuildRequires:  systemd-units

Provides:       kernel-modules >= %{kmod_kernel_version}.%{_arch}
Provides:       kmod-%{kmod_name} = %{?epoch:%{epoch}:}%{version}-%{release}

Requires(post): %{_sbindir}/weak-modules
Requires(postun):       %{_sbindir}/weak-modules
Requires:       kernel >= %{kmod_kernel_version}
Requires:       kernel-core-uname-r >= %{kmod_kernel_version}
Requires:       i2c-tools smartmontools dmidecode

%description
This package provides the %{kmod_name} kernel module(s) for ugreen_leds_controller.
It is built to depend upon the specific ABI provided by a range of releases
of the same variant of the Linux kernel and not on any one specific build.


%prep
%setup -q -n ugreen_leds_controller
echo "override %{kmod_name} * weak-updates/%{kmod_name}" > kmod-%{kmod_name}.conf

# Apply patch(es)

%build
pushd kmod
%{__make} -C %{kernel_source} %{?_smp_mflags} modules M=$PWD
popd

whitelist="/lib/modules/kabi-current/kabi_stablelist_%{_target_cpu}"
for modules in $( find . -name "*.ko" -type f -printf "%{findpat}\n" | sed 's|\.ko$||' | sort -u ) ; do
        # update greylist
        nm -u ./$modules.ko | sed 's/.*U //' |  sed 's/^\.//' | sort -u | while read -r symbol; do
                grep -q "^\s*$symbol\$" $whitelist || echo "$symbol" >> ./greylist
        done
done
sort -u greylist | uniq > greylist.txt

%install
%{__install} -d %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} kmod/%{kmod_name}.ko %{buildroot}/lib/modules/%{kmod_kernel_version}.%{_arch}/extra/%{kmod_name}/
%{__install} -d %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -m 0644 kmod-%{kmod_name}.conf %{buildroot}%{_sysconfdir}/depmod.d/
%{__install} -d %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m 0644 greylist.txt %{buildroot}%{_defaultdocdir}/kmod-%{kmod_name}-%{version}/
%{__install} -m 0644 scripts/ugreen-leds.conf %{buildroot}%{_sysconfdir}/

mkdir -p %{buildroot}/etc/modules-load.d/
%{__install} -m 0644 SPECS/ugreen-led.conf %{buildroot}/etc/modules-load.d/

mkdir -p %{buildroot}%{_bindir}/

%{__install} -m 0755 scripts/ugreen-diskiomon  %{buildroot}%{_bindir}/
%{__install} -m 0755 scripts/ugreen-netdevmon  %{buildroot}%{_bindir}/
%{__install} -m 0755 scripts/ugreen-probe-leds %{buildroot}%{_bindir}/

mkdir -p %{buildroot}%{_unitdir}/
%{__install} -m 0644 scripts/ugreen-netdevmon@.service  %{buildroot}%{_unitdir}/
%{__install} -m 0644 scripts/ugreen-diskiomon.service   %{buildroot}%{_unitdir}/

# strip the modules(s)
find %{buildroot} -type f -name \*.ko -exec %{__strip} --strip-debug \{\} \;

# Sign the modules(s)
%if %{?_with_modsign:1}%{!?_with_modsign:0}
	# If the module signing keys are not defined, define them here.
	%{!?privkey: %define privkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.priv}
	%{!?pubkey: %define pubkey %{_sysconfdir}/pki/SECURE-BOOT-KEY.der}
	for module in $(find %{buildroot} -type f -name \*.ko);
		do %{_usrsrc}/kernels/%{kmod_kernel_version}.%{_arch}/scripts/sign-file \
		sha256 %{privkey} %{pubkey} $module;
	done
%endif

%clean
%{__rm} -rf %{buildroot}

%post
modules=( $(find /lib/modules/%{kmod_kernel_version}.x86_64/extra/%{kmod_name} | grep '\.ko$') )
printf '%s\n' "${modules[@]}" | %{_sbindir}/weak-modules --add-modules --no-initramfs

mkdir -p "%{kver_state_dir}"
touch "%{kver_state_file}"

echo "systemctl start  ugreen-diskiomon.service"
echo "ls /sys/class/leds"
echo "Make sure you can see disk1, netdev, power, etc."
echo "systemctl enable ugreen-diskiomon.service"
echo
echo "to uninstall:"
echo "systemctl stop    ugreen-diskiomon.service"
echo "systemctl disable ugreen-diskiomon.service"

exit 0

%posttrans
# We have to re-implement part of weak-modules here because it doesn't allow
# calling initramfs regeneration separately
if [ -f "%{kver_state_file}" ]; then
        kver_base="%{kmod_kernel_version}"
        kvers=$(ls -d "/lib/modules/${kver_base%%.*}"*)

        for k_dir in $kvers; do
                k="${k_dir#/lib/modules/}"

                tmp_initramfs="/boot/initramfs-$k.tmp"
                dst_initramfs="/boot/initramfs-$k.img"

                # The same check as in weak-modules: we assume that the kernel present
                # if the symvers file exists.
                if [ -e "/$k_dir/symvers.gz" ]; then
                        /usr/bin/dracut -f "$tmp_initramfs" "$k" || exit 1
                        cmp -s "$tmp_initramfs" "$dst_initramfs"
                        if [ "$?" = 1 ]; then
                                mv "$tmp_initramfs" "$dst_initramfs"
                        else
                                rm -f "$tmp_initramfs"
                        fi
                fi
        done

        rm -f "%{kver_state_file}"
        rmdir "%{kver_state_dir}" 2> /dev/null
fi

rmdir "%{dup_state_dir}" 2> /dev/null

exit 0

%preun
if rpm -q --filetriggers kmod 2> /dev/null| grep -q "Trigger for weak-modules call on kmod removal"; then
        mkdir -p "%{kver_state_dir}"
        touch "%{kver_state_file}"
fi

mkdir -p "%{dup_state_dir}"
rpm -ql kmod-%{kmod_name}-%{version}-%{release}.%{_arch} | grep '\.ko$' > "%{dup_module_list}"

%postun
if rpm -q --filetriggers kmod 2> /dev/null| grep -q "Trigger for weak-modules call on kmod removal"; then
        initramfs_opt="--no-initramfs"
else
        initramfs_opt=""
fi

modules=( $(cat "%{dup_module_list}") )
rm -f "%{dup_module_list}"
printf '%s\n' "${modules[@]}" | %{_sbindir}/weak-modules --remove-modules $initramfs_opt

rmdir "%{dup_state_dir}" 2> /dev/null

exit 0

%files
%defattr(644,root,root,755)
/lib/modules/%{kmod_kernel_version}.%{_arch}/
%config /etc/depmod.d/kmod-%{kmod_name}.conf
%config(noreplace) %{_sysconfdir}/ugreen-leds.conf
%config(noreplace) /etc/modules-load.d/ugreen-led.conf
%doc /usr/share/doc/kmod-%{kmod_name}-%{version}/
%attr(0755, root, root) %{_bindir}/ugreen-diskiomon
%attr(0755, root, root) %{_bindir}/ugreen-netdevmon
%attr(0755, root, root) %{_bindir}/ugreen-probe-leds
%{_unitdir}/ugreen-netdevmon@.service
%{_unitdir}/ugreen-diskiomon.service

%changelog
* Sat Jun 22 2024 Axel Olmos <ax@olmosconsulting.com> - 0.0-14
- Rewritten to be a ugreen-led kmod spec.  Thank you Akemi for making the original template!

* Fri Nov 26 2021 Akemi Yagi <toracat@elrepo.org> - 0.0-1
- Initial build for RHEL 9
