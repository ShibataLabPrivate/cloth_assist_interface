% parseMocap.m: Matlab code to parse evart mocap trc files
% Author: Nishanth Koganti
% Date: 2015/08/22

% TODO:
% 1) Implement in python

function [Head, Data] = parseMocap(fileName)

fid = fopen(fileName);

% Getting the Header
fgets(fid);
fgets(fid);
fgets(fid);
fgets(fid);
Head = strsplit(fgets(fid),'\t');
Head = circshift(Head,[0,1]);
Head{1} = 'Frame';
Head{2} = 'Time';

% Getting the Data
Data = [];
while ~feof(fid)
      line = fgets(fid);
      dat = str2num(line);
      if (length(dat) == length(Head))
          Data = [Data; dat];
      else
          fprintf('Error!\n');
      end
end
Data(:,3:end) = Data(:,3:end)/1000;

fclose(fid);
