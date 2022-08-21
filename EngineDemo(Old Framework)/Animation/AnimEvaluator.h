#pragma once

#include <tuple>
#include <vector>

#include <assimp\Importer.hpp>
#include <assimp\scene.h>
#include <assimp\postprocess.h>
  
	
//��һ�鶯������������ʱ���ת������ֱ���ã����ʹ��AnimPlayer�ࡣ
class AnimEvaluator {
public:
	//���������Ĺ��췽���� �����ڶ�����������������ж��ǹ̶��ġ�
	// @param pAnim   Ϊ��������ƵĶ����� �������������Ȩ����������С�
	AnimEvaluator(const aiAnimation* pAnim);

	//The class destructor.
	~AnimEvaluator();

	//�������ʱ��Ķ��������������ͨ������GetTransformations()�����������̬��Ϊת������������м�����
	//@param pTime��Ҫ���㶯����ʱ�䣬����Ϊ��λ�� ����ӳ�䵽���������У����������������ֵ�� ���ʹ�ò������ӵ�ʱ�䡣
	void Evaluate(double pTime);

	//��������һ��Evaluate()����ʱ����ı任���� ��������aiAnimation��mChannels����ƥ�䡣
	const std::vector<aiMatrix4x4>& GetTransformations() const { return mTransforms; }

protected:
	const aiAnimation* mAnim;
	double mLastTime;
	std::vector<std::tuple<unsigned int, unsigned int, unsigned int> > mLastPositions;
	std::vector<aiMatrix4x4> mTransforms;
};