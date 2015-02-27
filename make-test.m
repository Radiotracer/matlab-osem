mex   -DWIN32 '-IC:\mip\include' '-LC:\mip\lib64' -llibmiputil.lib -llibcl.lib -llibirl.lib ... 
      -llibfftw3-3.lib -llibfftw3f-3.lib -llibfft-fftw3.lib -llibim.lib -llibimgio.lib  ...
     osem.c setup.c GetImages.c MeasToModPrj.c saveitercheck.c
 

clear; close all;
prj=readim('prj.im');
recon=osem('osem.par',prj);figure;imshow(recon,[]);title('recon');










 truth=readim('truth.im');
figure;imshow(truth,[]);title('truth');
% 
% recon0=osem('osem.par','prj.im','recon');
% figure;imshow(squeeze(recon0),[]);title('recon');



clear; close all;
recon=osem('osem.par','prj.im','recon');
figure;imshow(squeeze(recon),[]);title('recon');


prj=readim('prj.im');
recon1=osem('osem.par',prj,'recon');
figure;imshow(squeeze(recon1),[]);title('recon1');

figure;imshow(squeeze(recon-128*recon1),[]);title('difference');
